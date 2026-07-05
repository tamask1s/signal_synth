from setuptools import find_packages, setup


setup(
    name="synsigra",
    version="0.2.0",
    description="Synsigra local challenge loading and verification SDK",
    packages=find_packages(),
    python_requires=">=3.6",
    entry_points={"console_scripts": ["synsigra-verify=synsigra.cli:main"]},
)
