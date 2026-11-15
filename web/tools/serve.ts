// Minimal Bun static file server for the standalone Wasm widget demo (build/wasm/).
const BASE = new URL("../../build/wasm", import.meta.url).pathname;

const CONTENT_TYPES: Record<string, string> = {
  ".html": "text/html; charset=utf-8",
  ".js":   "application/javascript",
  ".wasm": "application/wasm",
  ".json": "application/json",
  ".css":  "text/css",
  ".svg":  "image/svg+xml",
};

const server = Bun.serve({
  port: 8080,
  async fetch(req: Request) {
    let path = new URL(req.url).pathname;
    if (path === "/") path = "/index.html";
    if (path.includes("..")) return new Response("Bad Request", { status: 400 });

    const file = Bun.file(`${BASE}${path}`);
    if (await file.exists()) {
      const ext = path.slice(path.lastIndexOf("."));
      const headers: Record<string, string> = { "Cache-Control": "no-cache" };
      if (CONTENT_TYPES[ext]) headers["Content-Type"] = CONTENT_TYPES[ext];
      return new Response(file, { headers });
    }
    return new Response("Not Found", { status: 404 });
  },
});

console.log(`FarmHorizon Web → http://localhost:${server.port}`);
