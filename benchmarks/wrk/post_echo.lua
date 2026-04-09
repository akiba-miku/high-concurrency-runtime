wrk.method = "POST"
wrk.body   = '{"hello":"runtime"}'

wrk.headers["Content-Type"] = "application/json"
wrk.headers["Connection"] = os.getenv("KEEP_ALIVE_HEADER") or "keep-alive"
