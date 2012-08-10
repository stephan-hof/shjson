from setuptools import setup
from distutils.core import Extension

mod = Extension(
    'shjson',
    sources=['shjson.c'],
    libraries=['yajl'],
    extra_compile_args=["-O3"],
    depends=["deque.c"]
    )

setup(
    name="shjson",
    version='0.1',
    description='Stream based JSON parsing with a Python C-Extension around YAJL',
    keywords='json',
    author='Stephan Hofmockel',
    author_email="Use the github issues",
    url="https://github.com/stephan-hof/shjson",
    packages=['tests'],
    ext_modules = [mod]
)

