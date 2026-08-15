package com.miniHls;

import com.sun.net.httpserver.HttpServer;
import com.sun.net.httpserver.HttpExchange;

import java.io.IOException;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.nio.file.Files;
import java.nio.file.Path;

public class Server {

    private static final int PORT = 8080;
    private static final Path SEGMENTS_DIR = Path.of("../segmenter");

    public static void main(String[] args) throws IOException {
        HttpServer server = HttpServer.create(new InetSocketAddress(PORT), 0);
        server.createContext("/", Server::handle);
        server.start();
        System.out.println("Serving " + SEGMENTS_DIR.toAbsolutePath() + " on port " + PORT);
    }

    private static void handle(HttpExchange exchange) throws IOException {
        String requested = exchange.getRequestURI().getPath().replaceFirst("^/", "");
        Path file = SEGMENTS_DIR.resolve(requested).normalize();

        if (!file.startsWith(SEGMENTS_DIR) || !Files.exists(file)) {
            exchange.sendResponseHeaders(404, -1);
            exchange.close();
            return;
        }

        exchange.getResponseHeaders().set("Content-Type", contentType(file));
        exchange.getResponseHeaders().set("Cache-Control", cacheControl(file));
        byte[] body = Files.readAllBytes(file);
        exchange.sendResponseHeaders(200, body.length);
        try (OutputStream os = exchange.getResponseBody()) {
            os.write(body);
        }
    }

    private static String contentType(Path file) {
        String name = file.toString();
        if (name.endsWith(".m3u8")) return "application/vnd.apple.mpegurl";
        if (name.endsWith(".ts")) return "video/mp2t";
        return "application/octet-stream";
    }

    private static String cacheControl(Path file) {
        String name = file.toString();
        if (name.endsWith(".m3u8")) return "no-cache";
        if (name.endsWith(".ts")) return "public, max-age=31536000, immutable";
        return "no-cache";
    }
}
