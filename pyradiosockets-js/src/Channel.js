import React from 'react';
import {Line} from 'react-chartjs-2';

import './App.css'

const args = (c) => ({
    fill: false,
    lineTension: 0.1,
    backgroundColor: c,
    borderColor: c,
    borderCapStyle: 'butt',
    borderDash: [],
    borderDashOffset: 0.0,
    borderJoinStyle: 'miter',
    pointBorderColor: c,
    pointBackgroundColor: '#ffffff',
    pointBorderWidth: 1,
    pointHoverRadius: 5,
    pointHoverBackgroundColor: c,
    pointHoverBorderColor: 'rgba(220,220,220,1)',
    pointHoverBorderWidth: 2,
    pointRadius: 1,
    pointHitRadius: 10,
})

const opts = {
    maintainAspectRatio: false,
    scales: {
        yAxes: [{
            ticks: {
                beginAtZero: 0,
                suggestedMin: 0.005,
                suggestedMax: 0.005
            }
        }]
    }
}



export default (props) => {
    return (
        <div className="Port">
            <h1>{props.report.key}</h1>
                <h2>Packet size TX: {Math.round(props.report.stats[props.report.stats.length - 1].tx_bits_packet_size / 8)}b / RX {Math.round(props.report.stats[props.report.stats.length - 1].rx_bits_packet_size / 8)}b </h2>
            <div className="Stat-container">
                <div className="Stat-container-row">
                    <Line data={{
                        labels: props.report.stats.map((_, i) => i),
                        datasets: [
                            {
                                ...args('rgba(155,102,192)'),
                                label: 'RX Mbps (sent)',
                                data: props.report.stats.map(r => r.other_tx_bits / 1000000)
                            },
                            {
                                ...args('rgba(75,192,192)'),
                                label: 'RX Mbps',
                                data: props.report.stats.map(r => r.rx_bits / 1000000)
                            }
                        ]
                    }} options={opts} height={400} />
                </div>
                <div className="Stat-container-row">
                    <Line data={{
                        labels: props.report.stats.map((_, i) => i),
                        datasets: [
                            /*{
                                ...args('rgba(155,155,155)'),
                                label: 'TX Mbps (estimated bw)',
                                data: props.report.stats.map(r => r.tx_bits / r.tx_dt / 1000000)
                            },*/
                            {
                                ...args('rgba(155,102,192)'),
                                label: 'TX Mbps',
                                data: props.report.stats.map(r => r.tx_bits / 1000000)
                            },
                            {
                                ...args('rgba(75,192,192)'),
                                label: 'TX Mbps (received)',
                                data: props.report.stats.map(r => r.other_rx_bits / 1000000)
                            }
                        ]
                    }} options={opts} height={400} />
                </div>
            </div>
        </div>
    )
}
