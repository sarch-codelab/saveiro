#!/usr/bin/env node
/* Servidor estático mínimo para la demo web de SaveiroOS. */

import http from "node:http";
import fs from "node:fs";
import path from "node:path";
import url from "node:url";

const root = path.resolve(process.argv[2] || "web");
const port = Number(process.argv[3] || process.env.PORT || 8080);

const types = {
    ".html": "text/html; charset=utf-8",
    ".css": "text/css; charset=utf-8",
    ".js": "text/javascript; charset=utf-8",
    ".mjs": "text/javascript; charset=utf-8",
    ".json": "application/json; charset=utf-8",
    ".wasm": "application/wasm",
    ".img": "application/octet-stream",
    ".bin": "application/octet-stream",
    ".svg": "image/svg+xml",
    ".png": "image/png",
    ".ico": "image/x-icon",
    ".txt": "text/plain; charset=utf-8",
};

const server = http.createServer((req, res) => {
    const parsed = url.parse(req.url);
    let pathname = decodeURIComponent(parsed.pathname || "/");

    if (pathname.endsWith("/")) {
        pathname += "index.html";
    }

    const filePath = path.join(root, path.normalize(pathname).replace(/^([.]{2}[\/\\])+/, ""));

    if (!filePath.startsWith(root)) {
        res.writeHead(403);
        res.end("forbidden");
        return;
    }

    fs.stat(filePath, (err, stats) => {
        if (err || !stats.isFile()) {
            res.writeHead(404, { "Content-Type": "text/plain; charset=utf-8" });
            res.end("404 " + pathname);
            return;
        }

        const ext = path.extname(filePath).toLowerCase();
        const headers = {
            "Content-Type": types[ext] || "application/octet-stream",
            "Content-Length": stats.size,
            "Cache-Control": "no-cache",
        };

        res.writeHead(200, headers);

        if (req.method === "HEAD") {
            res.end();
            return;
        }

        fs.createReadStream(filePath).pipe(res);
    });
});

server.listen(port, "0.0.0.0", () => {
    console.log(`SaveiroOS web en http://0.0.0.0:${port} (sirviendo ${root})`);
});
