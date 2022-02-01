# 3D PHP

A PoC extension that models a PHP runtime in 3D with [SketchUp](https://www.sketchup.com/).

## Requirements

- PHP 8.1+
- [SketchUp C API](https://extensions.sketchup.com/developers/sketchup_c_api/sketchup/index.html)
- macOS (The SketchUp C API only supports Windows and macOS)

## Installation

Download and extract the SketchUp C API and copy the SketchUpAPI framework to your local `Frameworks` directory.

```bash
$ mkdir -p ~/Library/Frameworks
$ cp -r /path/to/sketchup-sdk/SketchUpAPI.framework ~/Library/Frameworks
```

Install the extension.

```bash
$ phpize; \
  ./configure --enable-3d \
  make; \
  make test; \
  make install;
```

If you installed the SketchUpAPI framework in a non-standard path, you can specify the frameworks directory with `--with-sketchup-api=/path/to/frameworks/dir`.

## Usage

Once the extension is enabled, make a request with INI setting `php_3d.generate_model=1`. This will generate a SketchUp file with a 3D model of the request's runtime. Open the `.skp` file with SketchUp and enjoy the 3D PHP experience.
