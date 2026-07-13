#!/usr/bin/env python3
"""DeviceOps RAG service MVP.

This service intentionally uses only Python's standard library so it can run in
the current development container without dependency installation. The storage
backend is in-memory and replaceable by embedding + Milvus in a later stage.
"""

from __future__ import annotations

import json
import os
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any


DOCUMENTS: dict[str, dict[str, Any]] = {}
CHUNKS: list[dict[str, Any]] = []


def now_millis() -> int:
    return int(time.time() * 1000)


def chunk_text(content: str, chunk_size: int = 700, overlap: int = 120) -> list[str]:
    text = " ".join(content.split())
    if not text:
        return []
    chunks: list[str] = []
    start = 0
    while start < len(text):
        end = min(len(text), start + chunk_size)
        chunks.append(text[start:end])
        if end >= len(text):
            break
        start = max(0, end - overlap)
    return chunks


def score_chunk(chunk: dict[str, Any], query: str, device_type: str, error_code: str) -> float:
    query_l = query.lower()
    content_l = chunk["content"].lower()
    title_l = chunk["title"].lower()
    metadata = chunk.get("metadata", {})

    score = 0.0
    if query_l and query_l in title_l:
        score += 3.0
    if query_l and query_l in content_l:
        score += 1.0
    if error_code and metadata.get("error_code") == error_code:
        score += 2.0
    if device_type and metadata.get("device_type") == device_type:
        score += 1.5
    return score


def json_response(handler: BaseHTTPRequestHandler, status: int, payload: dict[str, Any]) -> None:
    body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
    handler.send_response(status)
    handler.send_header("Content-Type", "application/json; charset=utf-8")
    handler.send_header("Content-Length", str(len(body)))
    handler.end_headers()
    handler.wfile.write(body)


def read_json(handler: BaseHTTPRequestHandler) -> dict[str, Any]:
    length = int(handler.headers.get("Content-Length", "0"))
    if length <= 0:
        return {}
    raw = handler.rfile.read(length)
    return json.loads(raw.decode("utf-8"))


class RagHandler(BaseHTTPRequestHandler):
    server_version = "DeviceOpsRag/0.1"

    def do_GET(self) -> None:
        if self.path == "/health":
            json_response(self, 200, {"status": "ok", "documents": len(DOCUMENTS), "chunks": len(CHUNKS)})
            return
        json_response(self, 404, {"error": "not found"})

    def do_POST(self) -> None:
        try:
            if self.path == "/index":
                self.handle_index()
                return
            if self.path == "/retrieve":
                self.handle_retrieve()
                return
            if self.path == "/diagnose":
                self.handle_diagnose()
                return
            json_response(self, 404, {"error": "not found"})
        except json.JSONDecodeError:
            json_response(self, 400, {"error": "invalid json"})
        except Exception as exc:  # pragma: no cover - defensive handler
            json_response(self, 500, {"error": str(exc)})

    def log_message(self, fmt: str, *args: Any) -> None:
        print("[%s] %s" % (self.log_date_time_string(), fmt % args))

    def handle_index(self) -> None:
        payload = read_json(self)
        document_id = str(payload.get("document_id", "")).strip()
        title = str(payload.get("title", "")).strip()
        content = str(payload.get("content", "")).strip()
        metadata = payload.get("metadata") if isinstance(payload.get("metadata"), dict) else {}
        force_rebuild = bool(payload.get("force_rebuild", False))

        if not document_id or not title or not content:
            json_response(self, 400, {"error": "document_id, title and content are required"})
            return

        if force_rebuild:
            CHUNKS[:] = [item for item in CHUNKS if item["document_id"] != document_id]
        elif document_id in DOCUMENTS:
            json_response(self, 200, {"task_id": f"rag-index-{document_id}-{now_millis()}", "chunks": 0, "status": "skipped"})
            return

        DOCUMENTS[document_id] = {
            "document_id": document_id,
            "title": title,
            "content": content,
            "metadata": metadata,
            "indexed_at": now_millis(),
        }

        created = 0
        for index, chunk in enumerate(chunk_text(content)):
            CHUNKS.append({
                "document_id": document_id,
                "chunk_id": f"{document_id}#chunk-{index}",
                "title": title,
                "content": chunk,
                "metadata": metadata,
            })
            created += 1

        json_response(self, 200, {
            "task_id": f"rag-index-{document_id}-{now_millis()}",
            "chunks": created,
            "status": "indexed",
        })

    def handle_retrieve(self) -> None:
        payload = read_json(self)
        query = str(payload.get("query", "")).strip()
        device_type = str(payload.get("device_type", "")).strip()
        error_code = str(payload.get("error_code", "")).strip()
        top_k = int(payload.get("top_k") or 5)
        top_k = min(max(top_k, 1), 50)

        scored = []
        for chunk in CHUNKS:
            score = score_chunk(chunk, query, device_type, error_code)
            if score > 0:
                item = dict(chunk)
                item["score"] = score
                scored.append(item)
        scored.sort(key=lambda item: item["score"], reverse=True)

        json_response(self, 200, {"snippets": scored[:top_k]})

    def handle_diagnose(self) -> None:
        payload = read_json(self)
        event = payload.get("event", {})
        snippets = payload.get("knowledge_snippets", [])
        logs = payload.get("logs", [])

        title = event.get("title") or event.get("event_type") or "device fault"
        evidence = []
        for snippet in snippets[:3]:
            evidence.append(snippet.get("title", "knowledge"))
        for log in logs[:3]:
            evidence.append(log.get("message", "log"))

        summary = f"Diagnosis draft for {title}. "
        if evidence:
            summary += "Evidence: " + "; ".join(evidence)
        else:
            summary += "No external evidence was provided."

        json_response(self, 200, {
            "diagnosis": summary,
            "confidence": 0.5 if evidence else 0.2,
            "model": "deviceops-rag-mvp",
        })


def main() -> None:
    host = os.getenv("DEVICEOPS_RAG_HOST", "0.0.0.0")
    port = int(os.getenv("DEVICEOPS_RAG_PORT", "9601"))
    server = ThreadingHTTPServer((host, port), RagHandler)
    print(f"rag-service started: http://{host}:{port}")
    server.serve_forever()


if __name__ == "__main__":
    main()
