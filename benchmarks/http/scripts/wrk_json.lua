local paths = {}
for path in string.gmatch(os.getenv("BENCH_PATHS") or "/", "[^,]+") do
    table.insert(paths, path)
end
local request_index = 0

request = function()
    request_index = request_index + 1
    return wrk.format("GET", paths[((request_index - 1) % #paths) + 1])
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
        '"latency_p50_us":%.6f,"latency_p90_us":%.6f,' ..
        '"latency_p99_us":%.6f,"latency_p999_us":%.6f,' ..
        '"latency_max_us":%.6f,"errors":%d}\n',
        os.getenv("BENCH_SERVER"), os.getenv("BENCH_OPTIMIZATION"),
        os.getenv("BENCH_SCENARIO"), os.getenv("BENCH_PROFILE"),
        tonumber(os.getenv("BENCH_REPEAT")), tonumber(os.getenv("BENCH_CONNECTIONS")),
        tonumber(os.getenv("BENCH_DURATION")),
        summary.requests, request_rate, transfer_rate,
        latency.mean, latency.stdev, latency:percentile(50.0),
        latency:percentile(90.0), latency:percentile(99.0),
        latency:percentile(99.9), latency.max, errors
    ))
end
