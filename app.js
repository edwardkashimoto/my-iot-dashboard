/*
 * app.js
 * -----------------------------------------------------------------------
 * CBU Environmental Monitoring System — Dashboard Application Logic
 *
 * This file is entirely client-side JavaScript. It talks directly to
 * ThingSpeak's public REST API using the read-only key in config.js.
 * There is no backend server involved — this page can be hosted as a
 * static file on GitHub Pages, Netlify, Vercel, or any static host.
 * -----------------------------------------------------------------------
 */

(function () {
  "use strict";

  // -----------------------------------------------------------------------
  // STATE
  // -----------------------------------------------------------------------
  let charts = {};
  let currentRangeHours = 24;
  let lastFetchedFeeds = [];
  let sessionStart = Date.now();
  let refreshTimer = null;

  // -----------------------------------------------------------------------
  // INIT
  // -----------------------------------------------------------------------
  document.addEventListener("DOMContentLoaded", () => {
    initTheme();
    initClock();
    initRangeButtons();
    initExportButtons();
    initCharts();
    refreshAll();
    refreshTimer = setInterval(refreshAll, CONFIG.REFRESH_INTERVAL_MS);
    setInterval(updateUptime, 1000);
  });

  // =========================================================================
  // THEME (dark/light, persisted, defaults to system preference)
  // =========================================================================
  function initTheme() {
    const stored = localStorage.getItem("iot-dashboard-theme");
    const systemPrefersLight = window.matchMedia("(prefers-color-scheme: light)").matches;
    const theme = stored || (systemPrefersLight ? "light" : "dark");
    applyTheme(theme);

    document.getElementById("theme-toggle").addEventListener("click", () => {
      const next = document.documentElement.getAttribute("data-theme") === "dark" ? "light" : "dark";
      applyTheme(next);
      localStorage.setItem("iot-dashboard-theme", next);
    });
  }

  function applyTheme(theme) {
    document.documentElement.setAttribute("data-theme", theme);
    const icon = document.querySelector("#theme-toggle i");
    icon.className = theme === "dark" ? "fa-solid fa-moon" : "fa-solid fa-sun";
    // Redraw charts so gridline colors match the new theme.
    Object.values(charts).forEach((c) => c && c.update());
  }

  // =========================================================================
  // CLOCK
  // =========================================================================
  function initClock() {
    updateClock();
    setInterval(updateClock, 1000);
  }
  function updateClock() {
    const el = document.getElementById("header-clock");
    el.textContent = new Date().toLocaleTimeString([], { hour12: false });
  }

  function updateUptime() {
    const secs = Math.floor((Date.now() - sessionStart) / 1000);
    const h = String(Math.floor(secs / 3600)).padStart(2, "0");
    const m = String(Math.floor((secs % 3600) / 60)).padStart(2, "0");
    const s = String(secs % 60).padStart(2, "0");
    document.getElementById("health-uptime").textContent = `${h}:${m}:${s}`;
  }

  // =========================================================================
  // RANGE SELECTOR
  // =========================================================================
  function initRangeButtons() {
    document.querySelectorAll(".range-btn").forEach((btn) => {
      btn.addEventListener("click", () => {
        document.querySelectorAll(".range-btn").forEach((b) => b.classList.remove("active"));
        btn.classList.add("active");
        currentRangeHours = parseInt(btn.dataset.range, 10);
        refreshHistorical();
      });
    });
  }

  // =========================================================================
  // NETWORKING — ThingSpeak REST API
  // =========================================================================
  function tsUrl(path, params) {
    const base = `https://api.thingspeak.com/channels/${CONFIG.THINGSPEAK_CHANNEL_ID}${path}`;
    const query = new URLSearchParams({ api_key: CONFIG.THINGSPEAK_READ_API_KEY, ...params });
    return `${base}?${query.toString()}`;
  }

  async function fetchLatestData() {
    const res = await fetch(tsUrl("/feeds/last.json", {}));
    if (!res.ok) throw new Error(`ThingSpeak error ${res.status}`);
    return res.json();
  }

  async function fetchHistoricalData(hours) {
    // ThingSpeak accepts a "days" style range too; using minutes is more
    // flexible for the 1h/6h ranges while results= caps the payload.
    const results = hours <= 24 ? 1000 : 3000;
    const res = await fetch(
      tsUrl("/feeds.json", { minutes: hours * 60, results })
    );
    if (!res.ok) throw new Error(`ThingSpeak error ${res.status}`);
    return res.json();
  }

  // =========================================================================
  // MAIN REFRESH CYCLE
  // =========================================================================
  async function refreshAll() {
    try {
      const data = await fetchLatestData();
      updateDashboard(data);
      setCloudStatus("online");
    } catch (err) {
      console.error("Failed to fetch latest data:", err);
      setCloudStatus("offline");
      showFriendlyError();
    }
    refreshHistorical();
  }

  async function refreshHistorical() {
    try {
      const data = await fetchHistoricalData(currentRangeHours);
      lastFetchedFeeds = (data && data.feeds) || [];
      updateCharts(lastFetchedFeeds);
      updateAlerts(lastFetchedFeeds);
    } catch (err) {
      console.error("Failed to fetch historical data:", err);
    }
  }

  // =========================================================================
  // updateDashboard() — summary cards + device status + health strip
  // =========================================================================
  function updateDashboard(feed) {
    if (!feed || !feed.created_at) {
      showFriendlyError();
      return;
    }

    const f = CONFIG.FIELDS;
    const megaTemp = parseFloat(feed[f.MEGA_TEMPERATURE]);
    const megaHum = parseFloat(feed[f.MEGA_HUMIDITY]);
    const megaAQ = parseFloat(feed[f.MEGA_AIR_QUALITY]);
    const espTemp = parseFloat(feed[f.ESP32_TEMPERATURE]);
    const espHum = parseFloat(feed[f.ESP32_HUMIDITY]);
    const espAQ = parseFloat(feed[f.ESP32_AIR_QUALITY]);
    const statusCode = parseInt(feed[f.STATUS_CODE], 10) || 0;

    // Bit layout matches esp32_environment_gateway.ino buildStatusCode():
    // bit0 wifi, bit1 mega online, bit2 esp32 sensors valid, bit3 last upload ok
    const wifiOk = !!(statusCode & 0b0001);
    const megaOnline = !!(statusCode & 0b0010);
    const espSensorsOk = !!(statusCode & 0b0100);
    const lastUploadOk = !!(statusCode & 0b1000);

    const updatedAt = new Date(feed.created_at);

    // Prefer the Mega reading for the primary summary cards (it's the
    // reference sensor node); fall back to ESP32 if the Mega is offline
    // but the ESP32 has its own valid sensors.
    let primaryTemp, primaryHum, primaryAQ, primaryNode;
    if (megaOnline && !isNaN(megaTemp) && megaTemp !== 0) {
      primaryTemp = megaTemp; primaryHum = megaHum; primaryAQ = megaAQ;
      primaryNode = CONFIG.NODES.mega.name;
    } else if (espSensorsOk && !isNaN(espTemp) && espTemp !== 0) {
      primaryTemp = espTemp; primaryHum = espHum; primaryAQ = espAQ;
      primaryNode = CONFIG.NODES.esp32.name;
    }

    setReadingCard("temperature", primaryTemp, "°C", primaryNode, updatedAt, tempStatus(primaryTemp));
    setReadingCard("humidity", primaryHum, "%", primaryNode, updatedAt, humidityStatus(primaryHum));
    setReadingCard("air-quality", primaryAQ, "raw", primaryNode, updatedAt, airQualityStatus(primaryAQ));

    // Light intensity: neither node in this build has a physical light
    // sensor installed, so the dashboard truthfully reports N/A rather
    // than fabricating a reading.
    const anyLightSensor = CONFIG.NODES.mega.hasLightSensor || CONFIG.NODES.esp32.hasLightSensor;
    if (!anyLightSensor) {
      document.getElementById("card-light-value").textContent = "N/A";
      document.getElementById("card-light-status").textContent = "NO SENSOR";
    }

    document.getElementById("last-updated-note").textContent =
      `Last updated: ${updatedAt.toLocaleTimeString()}`;

    // Device status panel
    setStatusPill("mega", megaOnline ? "online" : "offline", megaOnline ? "ONLINE" : "OFFLINE");
    setStatusPill("esp32", wifiOk ? "online" : "offline", wifiOk ? "ONLINE" : "OFFLINE");
    setStatusPill("cloud", lastUploadOk ? "online" : "warning", lastUploadOk ? "CONNECTED" : "DEGRADED");

    // Health strip
    document.getElementById("health-last-sync").textContent = timeAgo(updatedAt);
    document.getElementById("health-rssi").textContent = wifiOk ? "OK" : "—";
  }

  function setReadingCard(metric, value, unit, node, updatedAt, status) {
    const valueEl = document.getElementById(`card-${metric}-value`);
    const statusEl = document.getElementById(`card-${metric}-status`);
    const nodeEl = document.getElementById(`card-${metric}-node`);
    const ageEl = document.getElementById(`card-${metric}-age`);

    if (value === undefined || value === null || isNaN(value)) {
      valueEl.innerHTML = `-- <span class="reading-unit">${unit}</span>`;
      statusEl.textContent = "NO DATA";
      statusEl.className = "reading-status-badge";
      if (nodeEl) nodeEl.textContent = "—";
      if (ageEl) ageEl.textContent = "—";
      return;
    }

    valueEl.innerHTML = `${value.toFixed(1)} <span class="reading-unit">${unit}</span>`;
    statusEl.textContent = status.label;
    statusEl.className = `reading-status-badge status-${status.level}`;
    if (nodeEl) nodeEl.textContent = node || "—";
    if (ageEl) ageEl.textContent = timeAgo(updatedAt);
  }

  function setStatusPill(nodeKey, level, label) {
    const pill = document.getElementById(`status-pill-${nodeKey}`);
    pill.textContent = label;
    pill.className = `status-pill status-${level}`;
  }

  function setCloudStatus(level) {
    const dot = document.querySelector("#cloud-indicator .status-dot");
    const label = document.querySelector("#cloud-indicator .status-label");
    dot.className = `status-dot status-${level === "online" ? "online" : "offline"}`;
    label.textContent = level === "online" ? "Cloud Connected" : "Cloud Unreachable";
  }

  function showFriendlyError() {
    document.getElementById("last-updated-note").textContent =
      "Unable to reach ThingSpeak right now — showing the last known values.";
  }

  // =========================================================================
  // STATUS CLASSIFICATION HELPERS (mirrors firmware thresholds)
  // =========================================================================
  function tempStatus(t) {
    if (t === undefined || isNaN(t)) return { label: "NO DATA", level: "" };
    const th = CONFIG.THRESHOLDS.TEMPERATURE;
    if (t > th.critical) return { label: "CRITICAL", level: "critical" };
    if (t > th.warning) return { label: "WARNING", level: "warning" };
    return { label: "NORMAL", level: "normal" };
  }

  function humidityStatus(h) {
    if (h === undefined || isNaN(h)) return { label: "NO DATA", level: "" };
    if (h < 20 || h > 80) return { label: "WARNING", level: "warning" };
    return { label: "NORMAL", level: "normal" };
  }

  function airQualityStatus(aq) {
    if (aq === undefined || isNaN(aq)) return { label: "NO DATA", level: "" };
    const th = CONFIG.THRESHOLDS.AIR_QUALITY;
    if (aq > th.warning) return { label: "DANGEROUS", level: "dangerous" };
    if (aq > th.moderate) return { label: "WARNING", level: "warning" };
    if (aq > th.good) return { label: "MODERATE", level: "moderate" };
    return { label: "GOOD", level: "good" };
  }

  function timeAgo(date) {
    const secs = Math.floor((Date.now() - date.getTime()) / 1000);
    if (secs < 5) return "just now";
    if (secs < 60) return `${secs}s ago`;
    const mins = Math.floor(secs / 60);
    if (mins < 60) return `${mins}m ago`;
    const hrs = Math.floor(mins / 60);
    return `${hrs}h ago`;
  }

  // =========================================================================
  // CHARTS
  // =========================================================================
  function chartTheme() {
    const isDark = document.documentElement.getAttribute("data-theme") === "dark";
    return {
      grid: isDark ? "rgba(255,255,255,0.06)" : "rgba(0,0,0,0.06)",
      text: isDark ? "#9FB2C2" : "#46596A",
    };
  }

  function baseLineOptions(extra) {
    const theme = chartTheme();
    return Object.assign(
      {
        responsive: true,
        maintainAspectRatio: false,
        animation: { duration: 300 },
        interaction: { mode: "index", intersect: false },
        plugins: {
          legend: { display: !!extra?.showLegend, labels: { color: theme.text, boxWidth: 12, font: { size: 11 } } },
          tooltip: { mode: "index", intersect: false },
        },
        scales: {
          x: { ticks: { color: theme.text, maxTicksLimit: 6 }, grid: { color: theme.grid } },
          y: { ticks: { color: theme.text }, grid: { color: theme.grid } },
        },
      },
      extra || {}
    );
  }

  function initCharts() {
    const ctxTemp = document.getElementById("chart-temperature").getContext("2d");
    charts.temperature = new Chart(ctxTemp, {
      type: "line",
      data: { labels: [], datasets: [lineDataset("Temperature (°C)", "#4FD1C5")] },
      options: baseLineOptions(),
    });

    const ctxHum = document.getElementById("chart-humidity").getContext("2d");
    charts.humidity = new Chart(ctxHum, {
      type: "line",
      data: { labels: [], datasets: [lineDataset("Humidity (%)", "#6FCF97")] },
      options: baseLineOptions(),
    });

    const ctxAQ = document.getElementById("chart-air-quality").getContext("2d");
    charts.airQuality = new Chart(ctxAQ, {
      type: "line",
      data: { labels: [], datasets: [lineDataset("Air Quality (raw)", "#F2A65A")] },
      options: baseLineOptions(),
    });

    const ctxCompTemp = document.getElementById("chart-compare-temperature").getContext("2d");
    charts.compareTemp = new Chart(ctxCompTemp, {
      type: "line",
      data: {
        labels: [],
        datasets: [
          lineDataset("Mega Station 1", "#4FD1C5"),
          lineDataset("ESP32 Station 1", "#F2A65A"),
        ],
      },
      options: baseLineOptions({ showLegend: true }),
    });

    const ctxCompHum = document.getElementById("chart-compare-humidity").getContext("2d");
    charts.compareHum = new Chart(ctxCompHum, {
      type: "line",
      data: {
        labels: [],
        datasets: [
          lineDataset("Mega Station 1", "#4FD1C5"),
          lineDataset("ESP32 Station 1", "#F2A65A"),
        ],
      },
      options: baseLineOptions({ showLegend: true }),
    });
  }

  function lineDataset(label, color) {
    return {
      label,
      data: [],
      borderColor: color,
      backgroundColor: color + "22",
      borderWidth: 2,
      pointRadius: 0,
      tension: 0.3,
      fill: true,
    };
  }

  function updateCharts(feeds) {
    if (!feeds || feeds.length === 0) return;
    const f = CONFIG.FIELDS;

    const labels = feeds.map((row) =>
      new Date(row.created_at).toLocaleString([], {
        month: "short", day: "numeric", hour: "2-digit", minute: "2-digit",
      })
    );

    setSeries(charts.temperature, labels, [numArr(feeds, f.MEGA_TEMPERATURE)]);
    setSeries(charts.humidity, labels, [numArr(feeds, f.MEGA_HUMIDITY)]);
    setSeries(charts.airQuality, labels, [numArr(feeds, f.MEGA_AIR_QUALITY)]);

    setSeries(charts.compareTemp, labels, [
      numArr(feeds, f.MEGA_TEMPERATURE),
      numArr(feeds, f.ESP32_TEMPERATURE),
    ]);
    setSeries(charts.compareHum, labels, [
      numArr(feeds, f.MEGA_HUMIDITY),
      numArr(feeds, f.ESP32_HUMIDITY),
    ]);
  }

  function numArr(feeds, fieldKey) {
    return feeds.map((row) => {
      const v = parseFloat(row[fieldKey]);
      return isNaN(v) ? null : v; // null leaves a gap rather than fabricating 0
    });
  }

  function setSeries(chart, labels, seriesArrays) {
    if (!chart) return;
    chart.data.labels = labels;
    seriesArrays.forEach((arr, i) => {
      if (chart.data.datasets[i]) chart.data.datasets[i].data = arr;
    });
    chart.update();
  }

  // =========================================================================
  // ALERTS — derived from historical feed data (temperature / air quality
  // crossing configured thresholds)
  // =========================================================================
  function updateAlerts(feeds) {
    const list = document.getElementById("alert-list");
    if (!feeds || feeds.length === 0) {
      list.innerHTML = `<li class="alert-empty">No alerts in the current data window.</li>`;
      return;
    }

    const f = CONFIG.FIELDS;
    const th = CONFIG.THRESHOLDS;
    const alerts = [];

    feeds.forEach((row) => {
      const time = new Date(row.created_at);
      const megaTemp = parseFloat(row[f.MEGA_TEMPERATURE]);
      const megaAQ = parseFloat(row[f.MEGA_AIR_QUALITY]);
      const espTemp = parseFloat(row[f.ESP32_TEMPERATURE]);
      const espAQ = parseFloat(row[f.ESP32_AIR_QUALITY]);

      if (!isNaN(megaTemp) && megaTemp > th.TEMPERATURE.critical) {
        alerts.push({ time, level: "critical", text: `High temperature — Mega Station 1: ${megaTemp.toFixed(1)}°C` });
      }
      if (!isNaN(espTemp) && espTemp > th.TEMPERATURE.critical) {
        alerts.push({ time, level: "critical", text: `High temperature — ESP32 Station 1: ${espTemp.toFixed(1)}°C` });
      }
      if (!isNaN(megaAQ) && megaAQ > th.AIR_QUALITY.warning) {
        alerts.push({ time, level: "warning", text: `Poor air quality — Mega Station 1: ${megaAQ.toFixed(0)} raw` });
      }
      if (!isNaN(espAQ) && espAQ > th.AIR_QUALITY.warning) {
        alerts.push({ time, level: "warning", text: `Poor air quality — ESP32 Station 1: ${espAQ.toFixed(0)} raw` });
      }
    });

    if (alerts.length === 0) {
      list.innerHTML = `<li class="alert-empty">No alerts in the current data window.</li>`;
      return;
    }

    // Most recent first, cap to last 15 shown
    alerts.sort((a, b) => b.time - a.time);
    const shown = alerts.slice(0, 15);

    list.innerHTML = shown
      .map(
        (a) => `
        <li class="alert-item">
          <span class="alert-time">${a.time.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" })}</span>
          <span class="alert-badge level-${a.level}">${a.level}</span>
          <span class="alert-text">${a.text}</span>
        </li>`
      )
      .join("");
  }

  // =========================================================================
  // EXPORT / PRINT
  // =========================================================================
  function initExportButtons() {
    document.getElementById("btn-export-csv").addEventListener("click", exportCsv);
    document.getElementById("btn-print").addEventListener("click", () => window.print());
  }

  function exportCsv() {
    if (!lastFetchedFeeds || lastFetchedFeeds.length === 0) {
      alert("No data currently loaded to export.");
      return;
    }
    const f = CONFIG.FIELDS;
    const header = [
      "timestamp", "mega_temperature", "mega_humidity", "mega_air_quality",
      "esp32_temperature", "esp32_humidity", "esp32_air_quality",
    ];
    const rows = lastFetchedFeeds.map((row) => [
      row.created_at,
      row[f.MEGA_TEMPERATURE], row[f.MEGA_HUMIDITY], row[f.MEGA_AIR_QUALITY],
      row[f.ESP32_TEMPERATURE], row[f.ESP32_HUMIDITY], row[f.ESP32_AIR_QUALITY],
    ]);
    const csv = [header, ...rows].map((r) => r.join(",")).join("\n");
    const blob = new Blob([csv], { type: "text/csv" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = `cbu-environment-data-${Date.now()}.csv`;
    a.click();
    URL.revokeObjectURL(url);
  }
})();
