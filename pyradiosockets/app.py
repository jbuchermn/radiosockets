import os
import asyncio

from .daemon import Daemon
from pyreact import PyreactApp

class App(PyreactApp):
    def __init__(self, daemon, start_server):
        super().__init__(
            os.path.join(
                os.path.dirname(os.path.realpath(__file__)),
                "..",
                "pyradiosockets-js"
            ) if start_server else None)
        self._daemon = daemon

        self._reports = {}

    async def update_reports(self):
        res = self._daemon.cmd_report()
        if res is not None:
            for r in res:
                if r['key'] not in self._reports:
                    self._reports[r['key']] = r
                    r['stats'] = [r['stats']]
                else:
                    for k in r:
                        if k != 'stats':
                            self._reports[r['key']][k] = r[k]
                    self._reports[r['key']]['stats'] += [r['stats']]
                    while len(self._reports[r['key']]['stats']) > 50:
                        self._reports[r['key']]['stats'].pop(0)

        await self.render()

    async def periodical_update(self, freq):
        while True:
            await self.update_reports()
            await asyncio.sleep(1. / freq)

    def _render(self):
        return {
            'reports': self._reports,
            'switch_channel': self._daemon.cmd_switch_channel,
            'update_port': self._daemon.cmd_update_port
        }

