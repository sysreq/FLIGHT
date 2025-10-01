#include "http_generator.h"
#include "../http_platform.h"
#include "../config/http_config.h"
#include <cstdio>
#include <cstring>

namespace http::ui {

size_t HttpGenerator::generate_page(char* buffer, size_t buffer_size, 
                                   uint64_t uptime_seconds, size_t event_queue_size) {
    return snprintf(buffer, buffer_size,
        "<html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<style>body{font-family:Arial;margin:20px;background:#f0f0f0;text-align:center}"
        ".btn{display:inline-block;padding:20px 40px;margin:10px;font-size:18px;"
        "background:#007bff;color:white;text-decoration:none;border-radius:4px;"
        "cursor:pointer;border:none}.btn:hover{background:#0056b3}"
        ".btn-stop{background:#dc3545}.btn-stop:hover{background:#c82333}"
        ".time-display{margin:20px;padding:15px;background:white;border-radius:4px;"
        "box-shadow:0 2px 4px rgba(0,0,0,0.1);font-size:16px}"
        ".timer-display{font-size:48px;font-weight:bold;color:#007bff;margin:20px}"
        ".status{font-size:14px;color:#666;margin:10px}"
        ".stats{display:inline-block;margin:5px 15px;padding:10px;background:#f8f9fa;"
        "border-radius:4px;font-size:12px}"
        ".telemetry{display:flex;justify-content:center;gap:20px;margin:30px auto}"
        ".telemetry-item{background:white;border-radius:8px;padding:20px;"
        "box-shadow:0 2px 4px rgba(0,0,0,0.1);min-width:150px}"
        ".telemetry-label{font-size:14px;color:#666;margin-bottom:5px}"
        ".telemetry-value{font-size:32px;font-weight:bold;color:#28a745}"
        ".telemetry-unit{font-size:16px;color:#999;margin-left:5px}</style>"
        "<script>"
        "var pollInterval=null,timerRunning=!1;"
        "function sendEvent(e){var t=Date.now(),n=Math.floor(t/1000),a=t%%1000;"
        "fetch('/?e='+e+'&v1='+n+'&v2='+a).then(function(){"
        "'start'===e?startPolling():'stop'===e&&stopPolling()}),updateTimeDisplay()}"
        "function updateTimeDisplay(){var e=new Date;"
        "document.getElementById('client-time').innerHTML='Client Time: '+e.toLocaleString()+'.'+e.getMilliseconds()}"
        "function updateStatus(){fetch('/status').then(function(e){return e.json()})"
        ".then(function(e){timerRunning=e.running;var t=e.elapsed,n=Math.floor(t/3600),"
        "a=Math.floor(t%%3600/60),l=t%%60,i=n.toString().padStart(2,'0')+':'+a.toString().padStart(2,'0')+':'+l.toString().padStart(2,'0');"
        "document.getElementById('timer').innerHTML=i;"
        "document.getElementById('status').innerHTML='Status: <strong>'+(e.running?'Running':'Stopped')+'</strong>';"
        "document.getElementById('stats').innerHTML='<span class=\"stats\">Start Count: '+(e.start_count||0)+'</span><span class=\"stats\">Stop Count: '+(e.stop_count||0)+'</span>';"
        "void 0!==e.speed&&(document.getElementById('speed-value').innerHTML=e.speed.toFixed(2));"
        "void 0!==e.altitude&&(document.getElementById('altitude-value').innerHTML=e.altitude.toFixed(1));"
        "void 0!==e.force&&(document.getElementById('force-value').innerHTML=e.force.toFixed(3));"
        "e.running&&!pollInterval?startPolling():!e.running&&pollInterval&&stopPolling()})"
        ".catch(function(e){console.error('Status fetch failed:',e);"
        "document.getElementById('status').innerHTML='Status: <em>Connection Error</em>'})}"
        "function startPolling(){pollInterval||(pollInterval=setInterval(updateStatus,500),updateStatus())}"
        "function stopPolling(){pollInterval&&(clearInterval(pollInterval),pollInterval=null),updateStatus()}"
        "setInterval(updateTimeDisplay,100);window.onload=function(){updateTimeDisplay(),updateStatus()};"
        "</script></head><body>"
        "<h1>Time Control & Telemetry</h1>"
        "<div class='telemetry'>"
        "<div class='telemetry-item'><div class='telemetry-label'>Speed</div>"
        "<div class='telemetry-value'><span id='speed-value'>%.2f</span><span class='telemetry-unit'>m/s</span></div></div>"
        "<div class='telemetry-item'><div class='telemetry-label'>Altitude</div>"
        "<div class='telemetry-value'><span id='altitude-value'>%.1f</span><span class='telemetry-unit'>m</span></div></div>"
        "<div class='telemetry-item'><div class='telemetry-label'>Force</div>"
        "<div class='telemetry-value'><span id='force-value'>%.3f</span><span class='telemetry-unit'>N</span></div></div>"
        "</div>"
        "<div class='timer-display' id='timer'>00:00:00</div>"
        "<div class='status' id='status'>Status: <em>Loading...</em></div>"
        "<div id='stats'></div>"
        "<div class='time-display' id='client-time'>Client Time: Loading...</div>"
        "<button onclick=\"sendEvent('start')\" class='btn'>Start</button>"
        "<button onclick=\"sendEvent('stop')\" class='btn btn-stop'>Stop</button>"
        "<div class='status'>System Uptime: %llu seconds | Event Queue: %zu items</div>"
        "</body></html>",
        HttpGenerator::speed, HttpGenerator::altitude, HttpGenerator::force, uptime_seconds, event_queue_size);
}

size_t HttpGenerator::generate_page(char* buffer, size_t buffer_size,
                                   uint64_t uptime_seconds, size_t event_queue_size,
                                   float speed_val, float altitude_val, float force_val) {
    return snprintf(buffer, buffer_size,
        "<html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<style>body{font-family:Arial;margin:20px;background:#f0f0f0;text-align:center}"
        ".btn{display:inline-block;padding:20px 40px;margin:10px;font-size:18px;"
        "background:#007bff;color:white;text-decoration:none;border-radius:4px;"
        "cursor:pointer;border:none}.btn:hover{background:#0056b3}"
        ".btn-stop{background:#dc3545}.btn-stop:hover{background:#c82333}"
        ".time-display{margin:20px;padding:15px;background:white;border-radius:4px;"
        "box-shadow:0 2px 4px rgba(0,0,0,0.1);font-size:16px}"
        ".timer-display{font-size:48px;font-weight:bold;color:#007bff;margin:20px}"
        ".status{font-size:14px;color:#666;margin:10px}"
        ".stats{display:inline-block;margin:5px 15px;padding:10px;background:#f8f9fa;"
        "border-radius:4px;font-size:12px}"
        ".telemetry{display:flex;justify-content:center;gap:20px;margin:30px auto}"
        ".telemetry-item{background:white;border-radius:8px;padding:20px;"
        "box-shadow:0 2px 4px rgba(0,0,0,0.1);min-width:150px}"
        ".telemetry-label{font-size:14px;color:#666;margin-bottom:5px}"
        ".telemetry-value{font-size:32px;font-weight:bold;color:#28a745}"
        ".telemetry-unit{font-size:16px;color:#999;margin-left:5px}</style>"
        "<script>"
        "var pollInterval=null,timerRunning=!1;"
        "function sendEvent(e){var t=Date.now(),n=Math.floor(t/1000),a=t%%1000;"
        "fetch('/?e='+e+'&v1='+n+'&v2='+a).then(function(){"
        "'start'===e?startPolling():'stop'===e&&stopPolling()}),updateTimeDisplay()}"
        "function updateTimeDisplay(){var e=new Date;"
        "document.getElementById('client-time').innerHTML='Client Time: '+e.toLocaleString()+'.'+e.getMilliseconds()}"
        "function updateStatus(){fetch('/status').then(function(e){return e.json()})"
        ".then(function(e){timerRunning=e.running;var t=e.elapsed,n=Math.floor(t/3600),"
        "a=Math.floor(t%%3600/60),l=t%%60,i=n.toString().padStart(2,'0')+':'+a.toString().padStart(2,'0')+':'+l.toString().padStart(2,'0');"
        "document.getElementById('timer').innerHTML=i;"
        "document.getElementById('status').innerHTML='Status: <strong>'+(e.running?'Running':'Stopped')+'</strong>';"
        "document.getElementById('stats').innerHTML='<span class=\"stats\">Start Count: '+(e.start_count||0)+'</span><span class=\"stats\">Stop Count: '+(e.stop_count||0)+'</span>';"
        "void 0!==e.speed&&(document.getElementById('speed-value').innerHTML=e.speed.toFixed(2));"
        "void 0!==e.altitude&&(document.getElementById('altitude-value').innerHTML=e.altitude.toFixed(1));"
        "void 0!==e.force&&(document.getElementById('force-value').innerHTML=e.force.toFixed(3));"
        "e.running&&!pollInterval?startPolling():!e.running&&pollInterval&&stopPolling()})"
        ".catch(function(e){console.error('Status fetch failed:',e);"
        "document.getElementById('status').innerHTML='Status: <em>Connection Error</em>'})}"
        "function startPolling(){pollInterval||(pollInterval=setInterval(updateStatus,500),updateStatus())}"
        "function stopPolling(){pollInterval&&(clearInterval(pollInterval),pollInterval=null),updateStatus()}"
        "setInterval(updateTimeDisplay,100);window.onload=function(){updateTimeDisplay(),updateStatus()};"
        "</script></head><body>"
        "<h1>Time Control & Telemetry</h1>"
        "<div class='telemetry'>"
        "<div class='telemetry-item'><div class='telemetry-label'>Speed</div>"
        "<div class='telemetry-value'><span id='speed-value'>%.2f</span><span class='telemetry-unit'>m/s</span></div></div>"
        "<div class='telemetry-item'><div class='telemetry-label'>Altitude</div>"
        "<div class='telemetry-value'><span id='altitude-value'>%.1f</span><span class='telemetry-unit'>m</span></div></div>"
        "<div class='telemetry-item'><div class='telemetry-label'>Force</div>"
        "<div class='telemetry-value'><span id='force-value'>%.3f</span><span class='telemetry-unit'>N</span></div></div>"
        "</div>"
        "<div class='timer-display' id='timer'>00:00:00</div>"
        "<div class='status' id='status'>Status: <em>Loading...</em></div>"
        "<div id='stats'></div>"
        "<div class='time-display' id='client-time'>Client Time: Loading...</div>"
        "<button onclick=\"sendEvent('start')\" class='btn'>Start</button>"
        "<button onclick=\"sendEvent('stop')\" class='btn btn-stop'>Stop</button>"
        "<div class='status'>System Uptime: %llu seconds | Event Queue: %zu items</div>"
        "</body></html>",
        speed_val, altitude_val, force_val, uptime_seconds, event_queue_size);
}

size_t HttpGenerator::generate_response(char* buffer, size_t buffer_size,
                                       uint64_t uptime_seconds, size_t event_queue_size) {
    return generate_response(buffer, buffer_size, uptime_seconds, event_queue_size,
                           speed, altitude, force);
}

size_t HttpGenerator::generate_response(char* buffer, size_t buffer_size,
                                       uint64_t uptime_seconds, size_t event_queue_size,
                                       float speed_val, float altitude_val, float force_val) {
    char content[8192];
    size_t content_len = generate_page(content, sizeof(content), uptime_seconds, event_queue_size,
                                      speed_val, altitude_val, force_val);

    return snprintf(buffer, buffer_size,
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: %zu\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Connection: close\r\n"
        "Cache-Control: no-cache, no-store, must-revalidate\r\n"
        "Pragma: no-cache\r\n"
        "Expires: 0\r\n"
        "\r\n%s",
        content_len, content);
}

} // namespace http::ui