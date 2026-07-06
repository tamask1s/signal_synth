from pathlib import Path

from setuptools import find_packages, setup

README = Path(__file__).with_name("README.md")

setup(
    name="synsigra",
    version="0.2.0",
    description="Local synthetic biosignal challenge loading and verification SDK",
    long_description=README.read_text(encoding="utf-8") if README.exists() else "",
    long_description_content_type="text/markdown",
    packages=find_packages(),
    include_package_data=True,
    package_data={"synsigra": ["py.typed"]},
    python_requires=">=3.8",
    entry_points={"console_scripts": ["synsigra-verify=synsigra.cli:main"]},
    zip_safe=False,
)
