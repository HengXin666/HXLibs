package bench;

import java.util.List;
import java.util.Map;
import org.springframework.boot.*;
import org.springframework.boot.autoconfigure.*;
import org.springframework.http.MediaType;
import org.springframework.core.io.FileSystemResource;
import org.springframework.core.io.Resource;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

@SpringBootApplication
@RestController
public class Application {
    private static final String ASSETS = System.getenv().getOrDefault("BENCH_ASSET_DIR", "benchmarks/http/assets");

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
    ResponseEntity<Resource> page() { return file("page.html", MediaType.TEXT_HTML); }

    @GetMapping(value = "/payload.bin", produces = MediaType.APPLICATION_OCTET_STREAM_VALUE)
    ResponseEntity<Resource> payload() { return file("payload.bin", MediaType.APPLICATION_OCTET_STREAM); }

    private ResponseEntity<Resource> file(String name, MediaType type) {
        Resource resource = new FileSystemResource(ASSETS + "/" + name);
        return ResponseEntity.ok().contentType(type).body(resource);
    }

    public static void main(String[] args) { SpringApplication.run(Application.class, args); }
}
