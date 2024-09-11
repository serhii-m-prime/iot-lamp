window.addEventListener('load', function () {
    Stats.init();
});

const Stats = {
    temperatureChart: null,
    humidityChart: null,
    batteryVoltageChart: null,
    IP: null,
    init: function () {
        this.displayDeviceData();
        setTimeout(async () => {
            this.drawTemperatureChart();
        }, 100);
        setTimeout(async () => {
            this.drawHumidityChart();
        }, 200);
        setTimeout(async () => {
            this.drawBatteryVoltageChart();
        }, 300);
        this.handleRefresh();
        this.handleButtons();
        this.handleAutoRefresh();
    },
    displayDeviceData: async function () {
        let response = await fetch('/status');
        let data = await response.json();
        Stats.IP = data.ip;
        if (data.hasOwnProperty('name')) {
            document.getElementById('name').innerHTML = `<a href="http://${data.ip}/stats" target="_blank">${data.name}</a>`;
        }
        if (data.hasOwnProperty('temperature')) {
            document.getElementById('current-t').innerText = data.temperature.toFixed(2) + " °C";
        }
        if (data.hasOwnProperty('humidity')) {
            document.getElementById('current-h').innerText = data.humidity.toFixed(2) + " %";
        }
        if (data.hasOwnProperty('batteryVoltage')) {
            document.getElementById('current-v').innerText = data.batteryVoltage.toFixed(2) + " V";
        }
        if (data.hasOwnProperty('lightLevel')) {
            document.getElementById('current-l').innerText = data.lightLevel.toFixed(2) + " Lx";
        }
    },
    drawTemperatureChart: async function () {
        const ctx = document.getElementById('tempChart');
        let response = await fetch('/temperature');
        let data = await response.json();
        let dataset = [];
        for (let i = 0; i < data.length; i++) {
            dataset.push({
                x: new Date(data[i][0] * 1000),
                y: data[i][1].toFixed(3)
            });
        }
        Stats.temperatureChart = new Chart(
            ctx,
            {
                type: 'line',
                data: {
                    datasets: [
                        {
                            label: 'Temperature over Time',
                            data: dataset,
                            borderColor: 'red',
                            backgroundColor: 'rgba(255, 0, 0, 0.15)',
                            fill: true,
                            tension: 0.1,
                        }
                    ]
                },
                options: {
                    scales: {
                        x: {
                            type: 'time',
                            time: {
                                unit: 'minute',
                                displayFormats: {
                                    minute: 'MMM d, HH:mm'
                                }
                            },
                            title: {
                                display: true,
                                text: 'Time'
                            }
                        },
                        y: {
                            title: {
                                display: true,
                                text: 'Temperature (°C)'
                            },
                            beginAtZero: true
                        }
                    }
                }
            }
        );
    },
    drawHumidityChart: async function () {
        const ctx = document.getElementById('humChart');
        let response = await fetch('/humidity');
        let data = await response.json();
        let dataset = [];
        for (let i = 0; i < data.length; i++) {
            dataset.push({
                x: new Date(data[i][0] * 1000),
                y: data[i][1].toFixed(3)
            });
        }
        Stats.humidityChart = new Chart(
            ctx,
            {
                type: 'line',
                data: {
                    datasets: [
                        {
                            label: 'Humidity over Time',
                            data: dataset,
                            borderColor: 'rgba(75, 192, 192, 1)',
                            backgroundColor: 'rgba(75, 192, 192, 0.2)',
                            fill: true,
                            tension: 0.1,
                        }
                    ]
                },
                options: {
                    scales: {
                        x: {
                            type: 'time',
                            time: {
                                unit: 'minute',
                                displayFormats: {
                                    minute: 'MMM d, HH:mm'
                                }
                            },
                            title: {
                                display: true,
                                text: 'Time'
                            }
                        },
                        y: {
                            title: {
                                display: true,
                                text: 'Humidity (%)'
                            },
                            beginAtZero: true
                        }
                    }
                }
            }
        );
    },
    drawBatteryVoltageChart: async function () {
        const ctx = document.getElementById('voltChart');
        let response = await fetch('/vbat');
        let data = await response.json();
        let dataset = [];
        for (let i = 0; i < data.length; i++) {
            dataset.push({
                x: new Date(data[i][0] * 1000),
                y: data[i][1].toFixed(3)
            });
        }
        Stats.batteryVoltageChart = new Chart(
            ctx,
            {
                type: 'line',
                data: {
                    datasets: [
                        {
                            label: 'Battery voltage over Time',
                            data: dataset,
                            borderColor: 'rgba(255, 165, 0, 1)',
                            backgroundColor: 'rgba(255, 165, 0, 0.2)',
                            fill: true,
                            tension: 0.1,
                        }
                    ]
                },
                options: {
                    scales: {
                        x: {
                            type: 'time',
                            time: {
                                unit: 'minute',
                                displayFormats: {
                                    minute: 'MMM d, HH:mm'
                                }
                            },
                            title: {
                                display: true,
                                text: 'Time'
                            }
                        },
                        y: {
                            title: {
                                display: true,
                                text: 'Voltage (V)'
                            },
                            beginAtZero: true
                        }
                    }
                }
            }
        );
    },
    handleRefresh: function () {
        const refreshButton = document.getElementById('refresh');
        refreshButton.addEventListener('click', async function () {
            await Stats.refresh();
        });
    },
    refresh: async function () {
        await Stats.displayDeviceData();
        setTimeout(async () => {
            let response = await fetch('/temperature');
            let newData = await response.json();
            let dataset = [];
            for (let i = 0; i < newData.length; i++) {
                dataset.push({
                    x: new Date(newData[i][0] * 1000),
                    y: newData[i][1].toFixed(3)
                });
            }
            Stats.temperatureChart.data.datasets[0].data = dataset;
            Stats.temperatureChart.update();
        }, 300);
        setTimeout(async () => {
            let response = await fetch('/humidity');
            let newData = await response.json();
            let dataset = [];
            for (let i = 0; i < newData.length; i++) {
                dataset.push({
                    x: new Date(newData[i][0] * 1000),
                    y: newData[i][1].toFixed(3)
                });
            }
            Stats.humidityChart.data.datasets[0].data = dataset;
            Stats.humidityChart.update();
        }, 600);
        setTimeout(async () => {
            let response = await fetch('/vbat');
            let newData = await response.json();
            let dataset = [];
            for (let i = 0; i < newData.length; i++) {
                dataset.push({
                    x: new Date(newData[i][0] * 1000),
                    y: newData[i][1].toFixed(3)
                });
            }
            Stats.batteryVoltageChart.data.datasets[0].data = dataset;
            Stats.batteryVoltageChart.update();
        }, 900);
    },
    handleButtons: function () {
        const on = document.getElementById('on');
        const off = document.getElementById('off');
        const auto = document.getElementById('auto');
        on.addEventListener('click', function () {
            fetch(`http://${Stats.IP}/lightOn`);
        });
        off.addEventListener('click', function () {
            fetch(`http://${Stats.IP}/lightOff`);
        });
        auto.addEventListener('click', function () {
            fetch(`http://${Stats.IP}/lightAuto`);
        });
    },
    handleAutoRefresh: function () {
        console.log("Auto refresh enabled");
        let updateInterval = setInterval(() => {
            if (document.visibilityState === "visible") {
                Stats.refresh();
            }
        }, 60000);
        document.addEventListener("visibilitychange", () => {
            if (document.visibilityState === "visible") {
                Stats.refresh();
                console.log("Auto refresh enabled");
                updateInterval = setInterval(() => {
                    if (document.visibilityState === "visible") {
                        Stats.refresh();
                    }
                }, 60000);
            } else {
                console.log("Auto refresh disabled");
                clearInterval(updateInterval);
            }
        });
    }
};