#!/usr/bin/env python
import sys
import os
import logging
from logging.handlers import RotatingFileHandler
import pytest

if __name__ == '__main__':
    # Show output results from every test function
    # Show the message output for skipped and expected failures
    args = ['-v', '-vrxs']

    # Add extra arguments
    if len(sys.argv) > 1:
        args.extend(sys.argv[1:])

    txt = 'pytest arguments: {}'.format(args)
    print(txt)

    sys.exit(pytest.main(args))
