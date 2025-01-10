# SPDX-License-Identifier: GPL-2.0+

from setuptools import setup
setup(name='patman',
      version='1.0',
      scripts=['patman'],
      packages=['patman'],
      package_dir={'patman': ''},
      package_data={'patman': ['README.rst']},
      classifiers=['Environment :: Console',
                   'Topic :: Software Development'])
