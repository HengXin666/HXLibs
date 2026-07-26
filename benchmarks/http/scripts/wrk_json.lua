local paths = {}
for path in string.gmatch(os.getenv("BENCH_PATHS") or "/", "[^,]+") do
    table.insert(paths, path)
end

if #paths == 1 then
    -- 单一路径: 只改写 wrk 的静态请求缓冲区, 请求生成路径上零 Lua 开销
    wrk.path = paths[1]
else
    -- 混合路径: 每线程预编码好全部请求, request() 仅做一次取模与查表
    local requests = {}
    local count = #paths
    local index = 0

    init = function()
        for i = 1, count do
            requests[i] = wrk.format(nil, paths[i])
        end
    end

    request = function()
        index = index + 1
        return requests[index % count + 1]
    end
end

done = function(summary, latency, requests)
    local duration_seconds = summary.duration / 1000000
    local request_rate = summary.requests / duration_seconds
    local transfer_rate = summary.bytes / duration_seconds
    local errors = summary.errors.connect + summary.errors.read +
                   summary.errors.write + summary.errors.status +
                   summary.errors.timeout

    io.write(string.format(
        '{"server":"%s","optimization":"%s","scenario":"%s","profile":"%s",' ..
        '"repeat":%d,"connections":%d,"duration_seconds":%d,"requests":%d,' ..
        '"requests_per_second":%.6f,"bytes_per_second":%.6f,' ..
        '"latency_mean_us":%.6f,"latency_stdev_us":%.6f,' ..
        '"latency_p50_us":%.6f,"latency_p75_us":%.6f,' ..
        '"latency_p90_us":%.6f,"latency_p95_us":%.6f,' ..
        '"latency_p99_us":%.6f,"latency_p999_us":%.6f,"latency_p9999_us":%.6f,' ..
        '"latency_max_us":%.6f,"errors":%d}\n',
        os.getenv("BENCH_SERVER"), os.getenv("BENCH_OPTIMIZATION"),
        os.getenv("BENCH_SCENARIO"), os.getenv("BENCH_PROFILE"),
        tonumber(os.getenv("BENCH_REPEAT")), tonumber(os.getenv("BENCH_CONNECTIONS")),
        tonumber(os.getenv("BENCH_DURATION")),
        summary.requests, request_rate, transfer_rate,
        latency.mean, latency.stdev,
        latency:percentile(50.0), latency:percentile(75.0),
        latency:percentile(90.0), latency:percentile(95.0),
        latency:percentile(99.0), latency:percentile(99.9),
        latency:percentile(99.99), latency.max, errors
    ))
end
