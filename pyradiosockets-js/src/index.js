import React from 'react';
import ReactDOM from 'react-dom';
import './index.css';
import App from './App';
import { PyreactRoot } from 'pyreact-js';

ReactDOM.render(
  <React.StrictMode>
    <PyreactRoot>
        {(props) => <App {...props} />}
    </PyreactRoot>
  </React.StrictMode>,
  document.getElementById('root')
);
