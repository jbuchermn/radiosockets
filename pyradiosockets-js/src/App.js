import React from 'react';

import Port from './Port';
import Channel from './Channel';
import Status from './Status';

import './App.css';

class App extends React.Component {
    constructor(props) {
        super(props);
    }

    render() {
        if (!this.props.reports) {
            return (
                <div className="App" >
                    Loading...
                </div>
            );
        }
        return (
            <div className="App" >
                {Object.values(this.props.reports).map(r => (
                    r.kind == 'port' ?
                        <Port report={r}
                            switchChannel={this.props.switch_channel}
                            appReport={this.props.reports["A" + r.id]} /> :
                        r.kind == 'status' ?
                            <Status report={r} command={this._command} /> :
                            r.kind == 'channel' ?
                                <Channel report={r} command={this._command} /> : null
                ))}
            </div>
        );
    }
}

export default App;
