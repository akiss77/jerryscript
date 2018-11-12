import re

from fuzzinator.config import config_get_callable
from fuzzinator.formatter import JsonFormatter
from fuzzinator.listener import EventListener


class CiListener(EventListener):

    NON_ALPHANUM = re.compile('[^0-9a-zA-Z]+')

    def __init__(self, config, issue_file_pattern=None):
        super().__init__(config)
        self._issue_file_pattern = issue_file_pattern
        self._jobs = {}

    def new_fuzz_job(self, ident, fuzzer, sut, cost, batch):
        self._jobs[ident] = 'f'

    def new_reduce_job(self, ident, sut, cost, issue_id, size):
        self._jobs[ident] = 'r'

    def new_update_job(self, ident, sut):
        self._jobs[ident] = 'u'

    def new_validate_job(self, ident, sut, issue_id):
        self._jobs[ident] = 'v'

    def activate_job(self, ident):
        self._progress('(' + self._jobs[ident])

    def job_progress(self, ident, progress):
        self._progress('.')

    def remove_job(self, ident):
        self._progress(')')
        del self._jobs[ident]

    def new_issue(self, issue):
        self._progress('+')
        self._save(issue)

    def invalid_issue(self, issue):
        self._progress('-')

    def update_issue(self, issue):
        self._progress('o')
        self._save(issue)

    def _progress(self, marker):
        print(marker, end='', flush=True)

    def _save(self, issue):
        if not self._issue_file_pattern:
            return

        formatter = config_get_callable(self.config, 'sut.' + issue['sut'], ['ci_formatter', 'formatter'])[0] or JsonFormatter
        short = formatter(issue=issue, format='short')
        long = formatter(issue=issue)

        issue_file_name = self._issue_file_pattern.format(id=self.NON_ALPHANUM.sub('_', short))

        with open(issue_file_name, 'w') as f:
            f.write(long)
