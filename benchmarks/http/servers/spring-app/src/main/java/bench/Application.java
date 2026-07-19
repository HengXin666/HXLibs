package bench;
import org.springframework.boot.*; import org.springframework.boot.autoconfigure.*; import org.springframework.web.bind.annotation.*; import org.springframework.http.MediaType;
@SpringBootApplication @RestController public class Application { @GetMapping("/") String hello(){return "Hello World!";} @GetMapping(value="/page.html",produces=MediaType.TEXT_HTML_VALUE) String page(){return "<!doctype html><html><body><h1>HXLibs benchmark</h1></body></html>";} public static void main(String[] a){SpringApplication.run(Application.class,a);} }
