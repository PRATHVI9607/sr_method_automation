document.addEventListener('DOMContentLoaded', function() {
    let liveChart;
    const CHART_MAX_POINTS = 20; // Max number of data points to show on the chart

    function initializeChart() {
        const ctx = document.getElementById('liveChart').getContext('2d');
        liveChart = new Chart(ctx, {
            type: 'line',
            data: {
                labels: [],
                datasets: [{
                    label: 'Temperature (°C)',
                    data: [],
                    borderColor: 'rgba(21, 101, 192, 1)',
                    backgroundColor: 'rgba(21, 101, 192, 0.2)',
                    borderWidth: 2,
                    yAxisID: 'y',
                    tension: 0.4,
                    fill: true,
                }, {
                    label: 'Anomaly (0: Nominal, 1: Anomaly)',
                    data: [],
                    borderColor: 'rgba(211, 47, 47, 1)',
                    backgroundColor: 'rgba(211, 47, 47, 0.2)',
                    borderWidth: 2,
                    yAxisID: 'y1',
                    stepped: true,
                    fill: true,
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                scales: {
                    x: {
                        ticks: {
                            display: false // Hide x-axis labels to keep it clean
                        }
                    },
                    y: {
                        type: 'linear',
                        display: true,
                        position: 'left',
                        title: {
                            display: true,
                            text: 'Temperature (°C)'
                        }
                    },
                    y1: {
                        type: 'linear',
                        display: true,
                        position: 'right',
                        max: 1.5,
                        min: -0.5,
                        ticks: {
                            stepSize: 1
                        },
                        grid: {
                            drawOnChartArea: false, 
                        },
                        title: {
                            display: true,
                            text: 'Anomaly Status'
                        }
                    }
                }
            }
        });
    }
    
    function addDataToChart(label, tempData, anomalyData) {
        liveChart.data.labels.push(label);
        liveChart.data.datasets[0].data.push(tempData);
        liveChart.data.datasets[1].data.push(anomalyData);
        
        if (liveChart.data.labels.length > CHART_MAX_POINTS) {
            liveChart.data.labels.shift();
            liveChart.data.datasets.forEach((dataset) => {
                dataset.data.shift();
            });
        }
        liveChart.update();
    }

    async function updateSensorData() {
        try {
            const response = await fetch('/data');
            const data = await response.json();

            document.getElementById('temp-value').innerText = `${data.temperature.toFixed(2)} °C`;
            document.getElementById('level-value').innerText = `${data.waterLevel} %`;

            const pumpStatusEl = document.getElementById('pump-status');
            pumpStatusEl.innerText = data.pumpStatus;
            pumpStatusEl.style.fontWeight = data.pumpStatus === 'ON' ? 'bold' : 'normal';
            pumpStatusEl.style.color = data.pumpStatus === 'ON' ? '#0d47a1' : '#555';

            const anomalyStatusEl = document.getElementById('anomaly-status');
            anomalyStatusEl.innerText = data.anomalyStatus;
            anomalyStatusEl.className = data.anomalyStatus;
            document.getElementById('anomaly-value').innerText = `Value: ${data.anomalyValue}`;
            
            const time = new Date().toLocaleTimeString();
            addDataToChart(time, data.temperature, data.anomalyStatus === 'ANOMALY' ? 1 : 0);

        } catch (error) {
            console.error('Error fetching sensor data:', error);
        }
    }

    async function updateLogTable() {
        try {
            const response = await fetch('/log');
            const logs = await response.json();
            const tableBody = document.querySelector('#log-table tbody');
            tableBody.innerHTML = '';
            logs.forEach(log => {
                const row = `<tr>
                    <td>${log.timestamp}</td>
                    <td>${log.temperature}</td>
                    <td>${log.waterLevel}</td>
                    <td>${log.pumpStatus}</td>
                    <td>${log.anomalyStatus}</td>
                    <td>${log.anomalyValue}</td>
                </tr>`;
                tableBody.innerHTML += row;
            });
        } catch (error) {
            console.error('Error fetching log data:', error);
        }
    }

    window.clearLog = async function() {
        if (!confirm('Are you sure you want to clear the entire log file on the device? This action cannot be undone.')) {
            return;
        }
        try {
            const response = await fetch('/clear', { method: 'POST' });
            if(response.ok) {
                alert('Log file cleared successfully.');
                updateLogTable();
            } else {
                alert('Failed to clear log file.');
            }
        } catch(error) {
            console.error('Error clearing log:', error);
            alert('An error occurred while trying to clear the log.');
        }
    };

    initializeChart();
    setInterval(updateSensorData, 2000); // Update live data every 2 seconds
    setInterval(updateLogTable, 60000); // Update log table every minute
    updateSensorData(); // Initial call
    updateLogTable();   // Initial call
});