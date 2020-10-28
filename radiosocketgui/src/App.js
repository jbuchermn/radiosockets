import React from 'react';
import {Line} from 'react-chartjs-2';

import './App.css';

const Statistics = (props) => {
    return (
        <div style={{margin: 100 }}>
            <Line data={{
                labels: props.report.map(r => r.t),
                datasets: [
                    {
                        label: 'TX Mbps',
                        fill: false,
                        lineTension: 0.1,
                        backgroundColor: 'rgba(140,102,192,0.4)',
                        borderColor: 'rgba(150,102,192,1)',
                        borderCapStyle: 'butt',
                        borderDash: [],
                        borderDashOffset: 0.0,
                        borderJoinStyle: 'miter',
                        pointBorderColor: 'rgba(140,102,192,1)',
                        pointBackgroundColor: '#fff',
                        pointBorderWidth: 1,
                        pointHoverRadius: 5,
                        pointHoverBackgroundColor: 'rgba(140,102,192,1)',
                        pointHoverBorderColor: 'rgba(220,220,220,1)',
                        pointHoverBorderWidth: 2,
                        pointRadius: 1,
                        pointHitRadius: 10,
                        data: props.report.map(r => r.tx_bits / 1000000)
                    },
                    {
                        label: 'RX Mbps',
                        fill: false,
                        lineTension: 0.1,
                        backgroundColor: 'rgba(75,192,192,0.4)',
                        borderColor: 'rgba(75,192,192,1)',
                        borderCapStyle: 'butt',
                        borderDash: [],
                        borderDashOffset: 0.0,
                        borderJoinStyle: 'miter',
                        pointBorderColor: 'rgba(75,192,192,1)',
                        pointBackgroundColor: '#fff',
                        pointBorderWidth: 1,
                        pointHoverRadius: 5,
                        pointHoverBackgroundColor: 'rgba(75,192,192,1)',
                        pointHoverBorderColor: 'rgba(220,220,220,1)',
                        pointHoverBorderWidth: 2,
                        pointRadius: 1,
                        pointHitRadius: 10,
                        data: props.report.map(r => r.rx_bits / 1000000)
                    }
                ]
            }} options={{
                scales: {
                    yAxes: [{
                        ticks: {
                            beginAtZero: 0,
                            suggestedMin: 0.5,
                            suggestedMax: 5
                        }
                    }]
                }
            }
            } /></div>
    );
}

class App extends React.Component {
    constructor(props, context) {
        super(props, context);
        this.state = {reports: {}}
        setInterval(() => {
            fetch('/cmd', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({'cmd': 'report'})
            }).then(p => p.json()).then(report => this._update(report));
        }, 600);
    }

    _update(report) {
        let reports = this.state.reports;

        report.forEach(r => {
            if (!reports[r.title]) {
                reports[r.title] = [];
            }

            reports[r.title].push({
                t: reports[r.title].length == 0 ? 0 : reports[r.title][reports[r.title].length - 1].t + 1,
                tx_bits: r.stats[0],
                tx_packet: r.stats[1],
                tx_errors: r.stats[2],
                rx_bits: r.stats[3],
                rx_packets: r.stats[4],
                rx_missed: r.stats[5],
                rx_dt: r.stats[6],
                other_rx_bits: r.stats[7],
                other_rx_packets: r.stats[8],
                other_rx_missed: r.stats[9],
                other_rx_dt: r.stats[10],
            })

            if(reports[r.title].length > 50){
                reports[r.title] = reports[r.title].slice(reports[r.title].length - 50);
            }
        });

        this.setState({reports});
    }

    render() {
        return (
            <div className="App" >
                {Object.keys(this.state.reports).map(key => (
                    <div>
                        <h1>{key}</h1>
                        <Statistics report={this.state.reports[key]} />
                    </div>
                ))}
            </div>
        );
    }
}

export default App;
