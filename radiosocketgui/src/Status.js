import React from 'react';
import {Line} from 'react-chartjs-2';

import './App.css';

const args = {
    fill: false,
    lineTension: 0.1,
    backgroundColor: 'rgba(75,192,192,0.4)',
    borderColor: 'rgba(75,192,192,1)',
    borderCapStyle: 'butt',
    borderDash: [],
    borderDashOffset: 0.0,
    borderJoinStyle: 'miter',
    pointBorderColor: 'rgba(75,192,192,1)',
    pointBackgroundColor: '#ffffff',
    pointBorderWidth: 1,
    pointHoverRadius: 5,
    pointHoverBackgroundColor: 'rgba(75,192,192,1)',
    pointHoverBorderColor: 'rgba(220,220,220,1)',
    pointHoverBorderWidth: 2,
    pointRadius: 1,
    pointHitRadius: 10,
}
const opts = {
    maintainAspectRatio: false,
    scales: {
        yAxes: [{
            ticks: {
                beginAtZero: 0,
                suggestedMin: 100
            }
        }]
    }
}


export default (props) => {
    return (
        <div className="Status">
            <h1>Status</h1>
            <div className="Stat-container">
                <div className="Stat-container-row">
                    <Line data={{
                        labels: props.report.stats.map(r => r.t),
                        datasets: [
                            {
                                ...args,
                                label: 'Usage',
                                data: props.report.stats.map(r => r.usage * 100)
                            }
                        ]
                    }} options={opts} height={400} />
                </div>
            </div>
        </div>
    )
}
