(this.webpackJsonpradiosocketgui=this.webpackJsonpradiosocketgui||[]).push([[0],{15:function(t,e,r){},152:function(t,e,r){"use strict";r.r(e);var a=r(0),o=r(2),n=r.n(o),s=r(41),i=r.n(s),c=(r(52),r(44)),d=r(1),b=r(42),l=r(43),p=r(46),u=r(45),j=r(5),h=(r(15),function(t){return{fill:!1,lineTension:.1,backgroundColor:t,borderColor:t,borderCapStyle:"butt",borderDash:[],borderDashOffset:0,borderJoinStyle:"miter",pointBorderColor:t,pointBackgroundColor:"#ffffff",pointBorderWidth:1,pointHoverRadius:5,pointHoverBackgroundColor:t,pointHoverBorderColor:"rgba(220,220,220,1)",pointHoverBorderWidth:2,pointRadius:1,pointHitRadius:10}}),m={maintainAspectRatio:!1,scales:{yAxes:[{ticks:{beginAtZero:0,suggestedMin:.5,suggestedMax:.5}}]}},f=function(t){return Object(a.jsxs)("div",{className:"Port",children:[Object(a.jsx)("h1",{children:t.report.title}),Object(a.jsxs)("h2",{children:["bound to channel C",t.report.bound]}),Object(a.jsxs)("div",{className:"Port-control",children:[Object(a.jsx)("div",{className:"Port-control-btn",onClick:function(){return t.command({cmd:"switch",port:t.report.id,new_channel:t.report.bound-12})},children:"--"}),Object(a.jsx)("div",{className:"Port-control-btn",onClick:function(){return t.command({cmd:"switch",port:t.report.id,new_channel:t.report.bound-1})},children:"-"}),Object(a.jsx)("div",{className:"Port-control-btn",onClick:function(){return t.command({cmd:"switch",port:t.report.id,new_channel:t.report.bound+1})},children:"+"}),Object(a.jsx)("div",{className:"Port-control-btn",onClick:function(){return t.command({cmd:"switch",port:t.report.id,new_channel:t.report.bound+12})},children:"++"})]}),Object(a.jsxs)("div",{className:"Stat-container",children:[Object(a.jsx)("div",{className:"Stat-container-row",children:Object(a.jsx)(j.Line,{data:{labels:t.report.stats.map((function(t){return t.t})),datasets:[Object(d.a)(Object(d.a)({},h("rgba(155,102,192)")),{},{label:"RX Mbps (sent)",data:t.report.stats.map((function(t){return t.other_tx_bits/1e6}))}),Object(d.a)(Object(d.a)({},h("rgba(75,192,192)")),{},{label:"RX Mbps",data:t.report.stats.map((function(t){return t.rx_bits/1e6}))})]},options:m,height:400})}),Object(a.jsx)("div",{className:"Stat-container-row",children:Object(a.jsx)(j.Line,{data:{labels:t.report.stats.map((function(t){return t.t})),datasets:[t.appReport?Object(d.a)(Object(d.a)({},h("rgba(155,100,100)")),{},{label:"TX Mbps (input)",data:t.appReport.stats.map((function(t){return t.tx_bits/1e6}))}):{},t.appReport?Object(d.a)(Object(d.a)({},h("rgba(155,0,0)")),{},{label:"TX Mbps (without skipped)",data:t.appReport.stats.map((function(t){return(1-t.tx_skipped)*t.tx_bits/1e6}))}):{},Object(d.a)(Object(d.a)({},h("rgba(155,102,192)")),{},{label:"TX Mbps",data:t.report.stats.map((function(t){return t.tx_bits/1e6}))}),Object(d.a)(Object(d.a)({},h("rgba(75,192,192)")),{},{label:"TX Mbps (received)",data:t.report.stats.map((function(t){return t.other_rx_bits/1e6}))})]},options:m,height:400})})]})]})},O=function(t){return{fill:!1,lineTension:.1,backgroundColor:t,borderColor:t,borderCapStyle:"butt",borderDash:[],borderDashOffset:0,borderJoinStyle:"miter",pointBorderColor:t,pointBackgroundColor:"#ffffff",pointBorderWidth:1,pointHoverRadius:5,pointHoverBackgroundColor:t,pointHoverBorderColor:"rgba(220,220,220,1)",pointHoverBorderWidth:2,pointRadius:1,pointHitRadius:10}},g={maintainAspectRatio:!1,scales:{yAxes:[{ticks:{beginAtZero:0,suggestedMin:.5,suggestedMax:.5}}]}},x=function(t){return Object(a.jsxs)("div",{className:"Port",children:[Object(a.jsx)("h1",{children:t.report.title}),Object(a.jsxs)("div",{className:"Stat-container",children:[Object(a.jsx)("div",{className:"Stat-container-row",children:Object(a.jsx)(j.Line,{data:{labels:t.report.stats.map((function(t){return t.t})),datasets:[Object(d.a)(Object(d.a)({},O("rgba(155,102,192)")),{},{label:"RX Mbps (sent)",data:t.report.stats.map((function(t){return t.other_tx_bits/1e6}))}),Object(d.a)(Object(d.a)({},O("rgba(75,192,192)")),{},{label:"RX Mbps",data:t.report.stats.map((function(t){return t.rx_bits/1e6}))})]},options:g,height:400})}),Object(a.jsx)("div",{className:"Stat-container-row",children:Object(a.jsx)(j.Line,{data:{labels:t.report.stats.map((function(t){return t.t})),datasets:[Object(d.a)(Object(d.a)({},O("rgba(155,102,192)")),{},{label:"TX Mbps",data:t.report.stats.map((function(t){return t.tx_bits/1e6}))}),Object(d.a)(Object(d.a)({},O("rgba(75,192,192)")),{},{label:"TX Mbps (received)",data:t.report.stats.map((function(t){return t.other_rx_bits/1e6}))})]},options:g,height:400})})]})]})},v={fill:!1,lineTension:.1,backgroundColor:"rgba(75,192,192,0.4)",borderColor:"rgba(75,192,192,1)",borderCapStyle:"butt",borderDash:[],borderDashOffset:0,borderJoinStyle:"miter",pointBorderColor:"rgba(75,192,192,1)",pointBackgroundColor:"#ffffff",pointBorderWidth:1,pointHoverRadius:5,pointHoverBackgroundColor:"rgba(75,192,192,1)",pointHoverBorderColor:"rgba(220,220,220,1)",pointHoverBorderWidth:2,pointRadius:1,pointHitRadius:10},k={maintainAspectRatio:!1,scales:{yAxes:[{ticks:{beginAtZero:0,suggestedMin:100}}]}},C=function(t){return Object(a.jsxs)("div",{className:"Status",children:[Object(a.jsx)("h1",{children:"Status"}),Object(a.jsx)("div",{className:"Stat-container",children:Object(a.jsx)("div",{className:"Stat-container-row",children:Object(a.jsx)(j.Line,{data:{labels:t.report.stats.map((function(t){return t.t})),datasets:[Object(d.a)(Object(d.a)({},v),{},{label:"Usage",data:t.report.stats.map((function(t){return 100*t.usage}))})]},options:k,height:400})})})]})},_=function(t){Object(p.a)(r,t);var e=Object(u.a)(r);function r(t){var a;return Object(b.a)(this,r),(a=e.call(this,t)).state={reports:{}},setInterval((function(){a._command({cmd:"report"}).then((function(t){return a._update(t)}))}),1e3),a}return Object(l.a)(r,[{key:"_command",value:function(t){return fetch("/cmd",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(t)}).then((function(t){return t.json()}))}},{key:"_update",value:function(t){var e=this,r={};console.log(t),t.forEach((function(t){var a=t.kind+t.id,o=Object(d.a)(Object(d.a)({},t),{},{stats:[].concat(Object(c.a)(e.state.reports[a]?e.state.reports[a].stats:[]),[Object(d.a)(Object(d.a)({},t.stats),{},{t:Math.round((new Date).getTime()/1e3)%100})])});o.stats.length>50&&(o.stats=o.stats.slice(o.stats.length-50)),r[a]=o})),this.setState({reports:r})}},{key:"render",value:function(){var t=this;return Object(a.jsx)("div",{className:"App",children:Object.values(this.state.reports).map((function(e){return"port"==e.kind?Object(a.jsx)(f,{report:e,command:t._command,appReport:t.state.reports["app"+e.id]}):"status"==e.kind?Object(a.jsx)(C,{report:e,command:t._command}):"channel"==e.kind?Object(a.jsx)(x,{report:e,command:t._command}):null}))})}}]),r}(n.a.Component);i.a.render(Object(a.jsx)(n.a.StrictMode,{children:Object(a.jsx)(_,{})}),document.getElementById("root"))},52:function(t,e,r){}},[[152,1,2]]]);
//# sourceMappingURL=main.88e60512.chunk.js.map