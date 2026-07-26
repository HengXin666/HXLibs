package bench;

import java.io.IOException;
import java.io.UncheckedIOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import java.util.Map;
import org.springframework.boot.*;
import org.springframework.boot.autoconfigure.*;
import org.springframework.core.io.FileSystemResource;
import org.springframework.core.io.Resource;
import org.springframework.http.MediaType;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

@SpringBootApplication
@RestController
public class Application {
    private static final String ASSETS = System.getenv().getOrDefault("BENCH_ASSET_DIR", "benchmarks/http/assets");
    // 静态资源启动时读入内存, 避免每请求的文件系统访问
    private static final byte[] PAGE = readAsset("page.html");
    private static final byte[] PAYLOAD = readAsset("payload.bin");

    private static byte[] readAsset(String name) {
        try {
            return Files.readAllBytes(Path.of(ASSETS, name));
        } catch (IOException error) {
            throw new UncheckedIOException(error);
        }
    }

    @GetMapping("/") String hello() { return "Hello World!"; }

    @GetMapping("/api/users") Map<String, Object> users() {
        return Map.of("users", List.of(
            Map.of("id", 1001, "name", "Alice", "active", true, "roles", List.of("admin", "editor")),
            Map.of("id", 1002, "name", "Bob", "active", true, "roles", List.of("viewer")),
            Map.of("id", 1003, "name", "Carol", "active", false, "roles", List.of("viewer", "billing"))
        ), "page", 1, "page_size", 20, "total", 3);
    }

    @GetMapping("/api/users/{userId}/orders/{orderId}")
    Map<String, Object> routeQuery(@PathVariable long userId, @PathVariable long orderId,
            @RequestParam int page, @RequestParam int limit, @RequestParam String sort) {
        return Map.of("user_id", userId, "order_id", orderId, "page", page,
                "limit", limit, "sort", sort);
    }

    @GetMapping(value = "/page.html", produces = MediaType.TEXT_HTML_VALUE)
    ResponseEntity<byte[]> page() {
        return ResponseEntity.ok().contentType(MediaType.TEXT_HTML).body(PAGE);
    }

    @GetMapping(value = "/payload.bin", produces = MediaType.APPLICATION_OCTET_STREAM_VALUE)
    ResponseEntity<byte[]> payload() {
        return ResponseEntity.ok().contentType(MediaType.APPLICATION_OCTET_STREAM).body(PAYLOAD);
    }

    // 磁盘变体: 用 Spring 原生 Resource 处理, 每请求真实读盘
    @GetMapping(value = "/page-file.html", produces = MediaType.TEXT_HTML_VALUE)
    ResponseEntity<Resource> pageFile() {
        return ResponseEntity.ok().contentType(MediaType.TEXT_HTML)
            .body(new FileSystemResource(ASSETS + "/files/page-file.html"));
    }

    @GetMapping(value = "/payload-file.bin", produces = MediaType.APPLICATION_OCTET_STREAM_VALUE)
    ResponseEntity<Resource> payloadFile() {
        return ResponseEntity.ok().contentType(MediaType.APPLICATION_OCTET_STREAM)
            .body(new FileSystemResource(ASSETS + "/files/payload-file.bin"));
    }

    public static void main(String[] args) { SpringApplication.run(Application.class, args); }
}
