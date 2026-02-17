#!/usr/bin/env python3
"""
Clipman - Custom Clipboard Manager Daemon
Ersetzt cliphist mit korrekter Multiline-Unterstuetzung
"""

import os
import sys
import json
import sqlite3
import hashlib
import subprocess
import socket
import threading
import uuid
import time
import signal
import re
from pathlib import Path
from datetime import datetime

# Konfiguration
CONFIG = {
    "max_items": 700,
    "max_image_size_mb": 10,
    "preview_length": 100,
    "socket_path": "/tmp/clipman.sock",
    "data_dir": Path.home() / ".local/share/clipman",
    "sensitive_ttl_seconds": 60,
}


class ClipmanDB:
    """SQLite database handler for clipboard metadata"""

    def __init__(self, db_path):
        self.db_path = db_path
        self.lock = threading.Lock()
        self.conn = sqlite3.connect(str(db_path), check_same_thread=False)
        self.conn.row_factory = sqlite3.Row
        self._init_db()

    def _init_db(self):
        self.conn.executescript('''
            CREATE TABLE IF NOT EXISTS items (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                uuid TEXT UNIQUE NOT NULL,
                content_type TEXT NOT NULL,
                preview TEXT,
                content_hash TEXT NOT NULL,
                file_path TEXT NOT NULL,
                thumb_path TEXT,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                is_favorite INTEGER DEFAULT 0,
                byte_size INTEGER,
                line_count INTEGER
            );
            CREATE INDEX IF NOT EXISTS idx_hash ON items(content_hash);
            CREATE INDEX IF NOT EXISTS idx_created ON items(created_at DESC);
            CREATE INDEX IF NOT EXISTS idx_favorite ON items(is_favorite);
        ''')
        self.conn.commit()

    def add_item(self, item_uuid, content_type, preview, content_hash,
                 file_path, thumb_path, byte_size, line_count):
        with self.lock:
            # Check duplicate by hash
            existing = self.conn.execute(
                "SELECT uuid FROM items WHERE content_hash = ?", (content_hash,)
            ).fetchone()

            if existing:
                # Update timestamp of existing item (move to top)
                self.conn.execute(
                    "UPDATE items SET created_at = CURRENT_TIMESTAMP WHERE uuid = ?",
                    (existing['uuid'],)
                )
                self.conn.commit()
                return existing['uuid']

            # Insert new item
            self.conn.execute('''
                INSERT INTO items (uuid, content_type, preview, content_hash,
                                 file_path, thumb_path, byte_size, line_count)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?)
            ''', (item_uuid, content_type, preview, content_hash,
                  file_path, thumb_path, byte_size, line_count))
            self.conn.commit()
            self._cleanup()
            return item_uuid

    def get_items(self, filter_type="all", favorites_only=False,
                  search="", limit=50):
        with self.lock:
            query = "SELECT * FROM items WHERE 1=1"
            params = []

            if filter_type == "text":
                query += " AND content_type = 'text'"
            elif filter_type == "image":
                query += " AND content_type = 'image'"

            if favorites_only or filter_type == "favorites":
                query += " AND is_favorite = 1"

            if search:
                query += " AND preview LIKE ?"
                params.append(f"%{search}%")

            query += " ORDER BY created_at DESC LIMIT ?"
            params.append(limit)

            return [dict(row) for row in self.conn.execute(query, params).fetchall()]

    def toggle_favorite(self, item_uuid):
        with self.lock:
            self.conn.execute(
                "UPDATE items SET is_favorite = NOT is_favorite WHERE uuid = ?",
                (item_uuid,)
            )
            self.conn.commit()

    def delete_item(self, item_uuid):
        with self.lock:
            row = self.conn.execute(
                "SELECT file_path, thumb_path FROM items WHERE uuid = ?",
                (item_uuid,)
            ).fetchone()

            if row:
                # Delete associated files
                for path in [row['file_path'], row['thumb_path']]:
                    if path:
                        full_path = CONFIG["data_dir"] / path
                        if full_path.exists():
                            try:
                                full_path.unlink()
                            except OSError:
                                pass

                self.conn.execute("DELETE FROM items WHERE uuid = ?", (item_uuid,))
                self.conn.commit()

    def clear_non_favorites(self):
        with self.lock:
            rows = self.conn.execute(
                "SELECT file_path, thumb_path FROM items WHERE is_favorite = 0"
            ).fetchall()

            for row in rows:
                for path in [row['file_path'], row['thumb_path']]:
                    if path:
                        full_path = CONFIG["data_dir"] / path
                        if full_path.exists():
                            try:
                                full_path.unlink()
                            except OSError:
                                pass

            self.conn.execute("DELETE FROM items WHERE is_favorite = 0")
            self.conn.commit()

    def _cleanup(self):
        """Remove oldest non-favorite items when exceeding max_items"""
        count = self.conn.execute("SELECT COUNT(*) FROM items").fetchone()[0]

        if count > CONFIG["max_items"]:
            excess = count - CONFIG["max_items"]
            old_items = self.conn.execute('''
                SELECT uuid FROM items WHERE is_favorite = 0
                ORDER BY created_at ASC LIMIT ?
            ''', (excess,)).fetchall()

            for row in old_items:
                # Call without lock since we're already in _cleanup (called from add_item which has lock)
                item_uuid = row['uuid']
                file_row = self.conn.execute(
                    "SELECT file_path, thumb_path FROM items WHERE uuid = ?",
                    (item_uuid,)
                ).fetchone()

                if file_row:
                    for path in [file_row['file_path'], file_row['thumb_path']]:
                        if path:
                            full_path = CONFIG["data_dir"] / path
                            if full_path.exists():
                                try:
                                    full_path.unlink()
                                except OSError:
                                    pass

                    self.conn.execute("DELETE FROM items WHERE uuid = ?", (item_uuid,))

            self.conn.commit()


class ContentStore:
    """File-based content storage for clipboard data"""

    def __init__(self, base_path):
        self.base_path = Path(base_path)
        (self.base_path / "text").mkdir(parents=True, exist_ok=True)
        (self.base_path / "images").mkdir(parents=True, exist_ok=True)
        (self.base_path / "thumbs").mkdir(parents=True, exist_ok=True)

    def store_text(self, content):
        """Store text content and return (uuid, file_path, hash)"""
        item_uuid = str(uuid.uuid4())
        content_bytes = content.encode('utf-8')
        content_hash = hashlib.sha256(content_bytes).hexdigest()
        file_path = f"text/{item_uuid}.txt"
        full_path = self.base_path / file_path
        full_path.write_text(content, encoding='utf-8')
        return item_uuid, file_path, content_hash

    def store_image(self, image_bytes):
        """Store image content and return (uuid, file_path, thumb_path, hash)"""
        item_uuid = str(uuid.uuid4())
        content_hash = hashlib.sha256(image_bytes).hexdigest()
        file_path = f"images/{item_uuid}.png"
        thumb_path = f"thumbs/{item_uuid}.png"

        full_path = self.base_path / file_path
        full_path.write_bytes(image_bytes)

        # Generate thumbnail
        try:
            from PIL import Image
            import io
            img = Image.open(io.BytesIO(image_bytes))
            img.thumbnail((100, 65))
            img.save(self.base_path / thumb_path)
        except ImportError:
            # Pillow not installed, skip thumbnail
            thumb_path = None
        except Exception:
            # Image processing failed
            thumb_path = None

        return item_uuid, file_path, thumb_path, content_hash

    def get_content(self, file_path):
        """Retrieve content from file"""
        full_path = self.base_path / file_path
        if not full_path.exists():
            return None

        if file_path.startswith("text/"):
            return full_path.read_text(encoding='utf-8')
        return full_path.read_bytes()


class ClipboardWatcher:
    """Watch clipboard changes via wl-paste"""

    def __init__(self, on_text, on_image):
        self.on_text = on_text
        self.on_image = on_image
        self.running = False
        self.last_text_hash = None
        self.last_image_hash = None
        self.text_proc = None

    def start(self):
        self.running = True
        # Watch text clipboard
        threading.Thread(target=self._watch_text, daemon=True).start()
        # Watch image clipboard
        threading.Thread(target=self._watch_image, daemon=True).start()

    def stop(self):
        self.running = False
        if self.text_proc:
            self.text_proc.terminate()

    @staticmethod
    def _run_with_timeout(cmd, timeout=2):
        """Run subprocess with proper cleanup on timeout (no zombie cat processes)"""
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            start_new_session=True,
        )
        try:
            stdout, _ = proc.communicate(timeout=timeout)
            return proc.returncode, stdout
        except subprocess.TimeoutExpired:
            os.killpg(proc.pid, signal.SIGKILL)
            proc.communicate()
            return None, None

    def _watch_text(self):
        """Poll for text clipboard changes"""
        while self.running:
            try:
                rc, stdout = self._run_with_timeout(["wl-paste", "--no-newline"])

                if rc == 0 and stdout:
                    content_hash = hashlib.sha256(stdout).hexdigest()

                    if content_hash != self.last_text_hash:
                        try:
                            text = stdout.decode('utf-8')
                            if text.strip():
                                self.last_text_hash = content_hash
                                self.on_text(text)
                        except UnicodeDecodeError:
                            pass

            except Exception as e:
                print(f"Text watcher error: {e}", file=sys.stderr)

            time.sleep(0.5)

    def _watch_image(self):
        """Poll for image clipboard changes"""
        while self.running:
            try:
                rc, stdout = self._run_with_timeout(["wl-paste", "--type", "image/png"])

                if rc == 0 and stdout:
                    content_hash = hashlib.sha256(stdout).hexdigest()

                    if content_hash != self.last_image_hash:
                        self.last_image_hash = content_hash
                        self.on_image(stdout)

            except Exception as e:
                print(f"Image watcher error: {e}", file=sys.stderr)

            time.sleep(0.5)


class IPCServer:
    """UNIX socket server for IPC commands"""

    def __init__(self, socket_path, db, store):
        self.socket_path = socket_path
        self.db = db
        self.store = store
        self.running = False
        self.server = None

    def start(self):
        self.running = True

        # Remove old socket if exists
        if os.path.exists(self.socket_path):
            os.unlink(self.socket_path)

        self.server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.server.bind(self.socket_path)
        self.server.listen(5)
        self.server.settimeout(1.0)  # Allow periodic check for shutdown

        print(f"Clipman IPC server listening on {self.socket_path}")

        while self.running:
            try:
                conn, _ = self.server.accept()
                threading.Thread(
                    target=self._handle_client,
                    args=(conn,),
                    daemon=True
                ).start()
            except socket.timeout:
                continue
            except Exception as e:
                if self.running:
                    print(f"Server error: {e}", file=sys.stderr)

    def stop(self):
        self.running = False
        if self.server:
            self.server.close()
        if os.path.exists(self.socket_path):
            os.unlink(self.socket_path)

    def _handle_client(self, conn):
        try:
            data = conn.recv(65536).decode('utf-8')
            request = json.loads(data)
            response = self._process_command(request)
            conn.send(json.dumps(response).encode('utf-8'))
        except Exception as e:
            error_response = {"status": "error", "error": str(e)}
            try:
                conn.send(json.dumps(error_response).encode('utf-8'))
            except Exception:
                pass
        finally:
            conn.close()

    def _process_command(self, request):
        cmd = request.get("cmd")
        args = request.get("args", {})

        if cmd == "list":
            items = self.db.get_items(
                filter_type=args.get("filter", "all"),
                favorites_only=args.get("favorites", False),
                search=args.get("search", ""),
                limit=args.get("limit", 50)
            )

            # Enrich items with full paths and normalized fields
            for item in items:
                if item.get("thumb_path"):
                    item["thumb"] = str(CONFIG["data_dir"] / item["thumb_path"])
                item["favorite"] = bool(item.get("is_favorite"))
                item["type"] = item.get("content_type")

            return {"status": "ok", "data": items}

        elif cmd == "paste":
            item_uuid = args.get("uuid")
            row = self.db.conn.execute(
                "SELECT file_path, content_type FROM items WHERE uuid = ?",
                (item_uuid,)
            ).fetchone()

            if row:
                content = self.store.get_content(row["file_path"])
                if content is None:
                    return {"status": "error", "error": "Content file not found"}

                if row["content_type"] == "text":
                    # Strip trailing whitespace from each line + trailing empty lines
                    if isinstance(content, str):
                        content = '\n'.join(line.rstrip() for line in content.split('\n'))
                        content = content.rstrip('\n')
                    elif isinstance(content, bytes):
                        content = b'\n'.join(line.rstrip() for line in content.split(b'\n'))
                        content = content.rstrip(b'\n')
                    subprocess.run(
                        ["wl-copy", "--"],
                        input=content.encode('utf-8') if isinstance(content, str) else content,
                        check=True
                    )
                else:
                    subprocess.run(
                        ["wl-copy", "--type", "image/png"],
                        input=content,
                        check=True
                    )
                return {"status": "ok"}

            return {"status": "error", "error": "Item not found"}

        elif cmd == "favorite":
            self.db.toggle_favorite(args.get("uuid"))
            return {"status": "ok"}

        elif cmd == "delete":
            self.db.delete_item(args.get("uuid"))
            return {"status": "ok"}

        elif cmd == "clear":
            self.db.clear_non_favorites()
            return {"status": "ok"}

        elif cmd == "ping":
            return {"status": "ok", "message": "pong"}

        return {"status": "error", "error": f"Unknown command: {cmd}"}


def is_sensitive(text: str) -> bool:
    """Detect password-like strings via heuristics.
    Rules: single-line, 8-128 chars, no spaces, >=3 of 4 char classes.
    Excludes: paths, URLs, hex colors, emails, pure numbers."""
    if '\n' in text or '\r' in text:
        return False
    t = text.strip()
    if not (8 <= len(t) <= 128):
        return False
    if ' ' in t:
        return False
    # Exclude known patterns
    if t.startswith(('/','~')):          # file paths
        return False
    if re.match(r'https?://', t):        # URLs
        return False
    if re.match(r'^#[0-9a-fA-F]{3,8}$', t):  # hex colors
        return False
    if '@' in t and '.' in t.split('@')[-1]:  # emails
        return False
    if t.isdigit():                      # pure numbers
        return False
    # Count character classes
    classes = sum([
        bool(re.search(r'[A-Z]', t)),
        bool(re.search(r'[a-z]', t)),
        bool(re.search(r'[0-9]', t)),
        bool(re.search(r'[^A-Za-z0-9]', t)),
    ])
    return classes >= 3


# Active sensitive-item timers (uuid -> Timer)
_sensitive_timers: dict[str, threading.Timer] = {}


def main():
    """Main entry point"""
    print("Clipman - Custom Clipboard Manager")
    print(f"Data directory: {CONFIG['data_dir']}")

    # Setup data directory
    CONFIG["data_dir"].mkdir(parents=True, exist_ok=True)

    # Initialize components
    db = ClipmanDB(CONFIG["data_dir"] / "clipman.db")
    store = ContentStore(CONFIG["data_dir"])
    server = IPCServer(CONFIG["socket_path"], db, store)

    def on_text(text):
        """Handle new text clipboard content"""
        item_uuid, file_path, content_hash = store.store_text(text)
        sensitive = is_sensitive(text)
        if sensitive:
            preview = "[sensitive] " + "\u2022" * min(len(text), 12)
        else:
            preview = text[:CONFIG["preview_length"]].replace('\n', ' ').replace('\t', ' ')
        line_count = text.count('\n') + 1
        stored_uuid = db.add_item(
            item_uuid, "text", preview, content_hash,
            file_path, None, len(text.encode('utf-8')), line_count
        )
        print(f"Stored text: {preview[:50]}...")

        if sensitive:
            def auto_delete():
                # Skip if user marked as favorite in the meantime
                with db.lock:
                    row = db.conn.execute(
                        "SELECT is_favorite FROM items WHERE uuid = ?",
                        (stored_uuid,)
                    ).fetchone()
                    if row and row['is_favorite']:
                        print(f"Sensitive item {stored_uuid[:8]} kept (favorite)")
                        return
                db.delete_item(stored_uuid)
                _sensitive_timers.pop(stored_uuid, None)
                print(f"Sensitive item {stored_uuid[:8]} auto-deleted after {CONFIG['sensitive_ttl_seconds']}s")

            # Cancel previous timer for same uuid (duplicate re-copy)
            old = _sensitive_timers.pop(stored_uuid, None)
            if old:
                old.cancel()
            t = threading.Timer(CONFIG["sensitive_ttl_seconds"], auto_delete)
            t.daemon = True
            t.start()
            _sensitive_timers[stored_uuid] = t
            print(f"Sensitive item detected, auto-delete in {CONFIG['sensitive_ttl_seconds']}s")

    def on_image(image_bytes):
        """Handle new image clipboard content"""
        if len(image_bytes) > CONFIG["max_image_size_mb"] * 1024 * 1024:
            print(f"Image too large ({len(image_bytes)} bytes), skipping")
            return

        item_uuid, file_path, thumb_path, content_hash = store.store_image(image_bytes)
        preview = f"[Image {len(image_bytes)//1024}KB]"
        db.add_item(
            item_uuid, "image", preview, content_hash,
            file_path, thumb_path, len(image_bytes), 0
        )
        print(f"Stored image: {preview}")

    # Start clipboard watcher
    watcher = ClipboardWatcher(on_text, on_image)
    watcher.start()

    # Handle shutdown signals
    def shutdown(signum, frame):
        print("\nShutting down...")
        watcher.stop()
        server.stop()
        sys.exit(0)

    signal.signal(signal.SIGINT, shutdown)
    signal.signal(signal.SIGTERM, shutdown)

    # Start IPC server (blocking)
    try:
        server.start()
    except KeyboardInterrupt:
        shutdown(None, None)


if __name__ == "__main__":
    main()
