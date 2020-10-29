import React from 'react';

import Port from './Port';
import Channel from './Channel';
import Status from './Status';

import './App.css';

class App extends React.Component {
    constructor(props) {
        super(props);

        this.state = {reports: {}}
        setInterval(() => {
            this._command({ 'cmd': 'report' }).then(reports => this._update(reports));
        }, 1000);
    }

    _command(cmd) {
        return fetch('/cmd', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(cmd)
        }).then(p => p.json());
    }

    _update(reports) {
        let new_reports = {}

        reports.forEach(r => {
            let unique = r.kind + r.id;
            let res = {
                ...r,
                stats: [
                    ...(this.state.reports[unique] ?
                        this.state.reports[unique].stats : []),
                    {...r.stats, t: Math.round(new Date().getTime()/1000)%100 }
                ]
            };

            if (res.stats.length > 50) {
                res.stats =
                    res.stats.slice(res.stats.length - 50);
            }

            new_reports[unique] = res;
        });

        this.setState({ reports: new_reports });
    }

    render() {
        return (
            <div className="App" >
                {Object.values(this.state.reports).map(r => (
                    r.kind == 'port' ? 
                        <Port report={r} command={this._command}
                            appReport={this.state.reports["app" + r.id]}/> : 
                    r.kind == 'status' ?
                        <Status report={r} command={this._command}/> :
                    r.kind == 'channel' ?
                        <Channel report={r} command={this._command}/> : null
                ))}
            </div>
        );
    }
}

export default App;
