# DeviceOps RAG Service

Python HTTP MVP for RAG indexing, retrieval and diagnosis generation.

```bash
DEVICEOPS_RAG_PORT=9601 python3 services/rag_service/server.py
```

Endpoints:

- `GET /health`
- `POST /index`
- `POST /retrieve`
- `POST /diagnose`

The MVP uses in-memory chunk storage and keyword scoring. It is designed to be
replaced by embedding + Milvus without changing the C++ service boundary.
