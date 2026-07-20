package bench;

import java.util.List;
import java.util.Map;
import org.springframework.boot.*;
import org.springframework.boot.autoconfigure.*;
import org.springframework.http.MediaType;
import org.springframework.web.bind.annotation.*;

@SpringBootApplication
@RestController
public class Application {
    private static final String HTML = """
        <!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>HXLibs 压测页面</title><style>body{font-family:system-ui;margin:0;background:#f4f7fb;color:#172033}.wrap{max-width:960px;margin:auto;padding:48px 24px}.hero{padding:40px;border-radius:20px;background:#fff;box-shadow:0 16px 50px #14213d18}h1{font-size:42px;margin:0 0 12px}.grid{display:grid;grid-template-columns:repeat(3,1fr);gap:16px;margin-top:28px}.card{padding:20px;border:1px solid #dce4ef;border-radius:14px}.tag{color:#2563eb;font-weight:700}@media(max-width:640px){.grid{grid-template-columns:1fr}}</style></head><body><main class="wrap"><section class="hero"><span class="tag">HTTP BENCHMARK</span><h1>面向真实流量的响应页面</h1><p>该页面包含常见的标题、布局、卡片与响应式样式，用于模拟服务端返回普通产品页面的场景。</p><div class="grid"><article class="card"><b>稳定</b><p>固定响应体确保实现之间可比较。</p></article><article class="card"><b>现代</b><p>包含实际 HTML 与 CSS 结构。</p></article><article class="card"><b>可观测</b><p>同时采集吞吐、延迟与错误。</p></article></div></section></main></body></html>
        """;
    private static final byte[] PAYLOAD = new byte[64 * 1024];

    @GetMapping("/") String hello() { return "Hello World!"; }

    @GetMapping("/api/users") Map<String, Object> users() {
        return Map.of("users", List.of(
            Map.of("id", 1001, "name", "Alice", "active", true, "roles", List.of("admin", "editor")),
            Map.of("id", 1002, "name", "Bob", "active", true, "roles", List.of("viewer")),
            Map.of("id", 1003, "name", "Carol", "active", false, "roles", List.of("viewer", "billing"))
        ), "page", 1, "page_size", 20, "total", 3);
    }

    @GetMapping(value = "/page.html", produces = MediaType.TEXT_HTML_VALUE)
    String page() { return HTML; }

    @GetMapping(value = "/payload.bin", produces = MediaType.APPLICATION_OCTET_STREAM_VALUE)
    byte[] payload() { return PAYLOAD; }

    public static void main(String[] args) { SpringApplication.run(Application.class, args); }
}
