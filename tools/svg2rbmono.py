"""
Quick script to convert svg to red/white and black/white images.

Waveshare gives a [description](https://www.waveshare.com/wiki/E-Paper_Floyd-Steinberg)
on how to convert pictures into Floyd-Steinberg scatter plots, which is using a color map
being read by Photoshop to convert the image, export a gif and finally convert to bitmap.

The process is using costly software for a simple task that can easily
be replaced by a few lines of python code.
The Black-White-Red.act defines 3 colors black (0,0,0), red(255,0,0) and white (255,255,255).
No need to read it here, a palette for conversion is defined manually.

This script creates two 1-bit bitmap images from an svg input file: one for the black pixels
and one for the red pixels (note: the red is inverted).

The code for importing svg-images is adapted from [sphinxext-photofinish](https://github.com/wpilibsuite/sphinxext-photofinish)
"""

# SPDX-License-Identifier: MIT
#
# Copyright (C) WPILib, 2024-2025 Robert Wendlandt
#

import os
import sys
import subprocess
from dataclasses import dataclass
from pathlib import Path
from shutil import which, copy
from typing import Optional, Union

Import("env")

try:
    from PIL import Image
except ModuleNotFoundError:
    env.Execute('"$PYTHONEXE" -m pip install pillow')
    from PIL import Image

class NoToolError(RuntimeError):
    """No tool for conversion found."""
    pass

class FailedConversionError(RuntimeError):
    """Conversion failed."""
    pass

def svg_to_png(
    svg_path: Union[str, Path],
    out_path: Union[str, Path],
    width: Optional[int] = None,
    height: Optional[int] = None,
):
    """
    Converts an svg to a png.
    This function makes many attempts to convert successfully.
    Order of trials:
    - cairosvg
    - svglib
    - System Inkscape
    - System rsvg-convert
    - System rsvg
    - System svgexport
    - System imagemagick (magick) - v7
    - System imagemagick (convert) - v6
    """

    svg_path = str(svg_path)
    out_path = str(out_path)

    if width is None and height is None:
        width = 200

    inkscape_args = [
        "--export-background-opacity=0",
        "--export-type=png",
        f"--export-filename={out_path}",
        str(svg_path),
    ]
    if width:
        inkscape_args += [f"--export-width={width}"]
    if height:
        inkscape_args += [f"--export-height={height}"]

    rsvg_convert_args = [svg_path, "-o", out_path]
    if width:
        rsvg_convert_args += [f"--width={width}"]
    if height:
        rsvg_convert_args += [f"--height={height}"]

    imagemagick_args = [
        svg_path,
        "-background",
        "none",
        out_path,
    ]  # v7 compatible?
    resize_arg = "-resize "
    if width:
        resize_arg += str(width)
    if height:
        resize_arg += "x"
        resize_arg += str(height)
    imagemagick_args += resize_arg.split()

    svgexport_args = [svg_path, out_path]
    size_arg = ""
    if width:
        size_arg += str(width)
    size_arg += ":"
    if height:
        size_arg += str(height)
    svgexport_args += [size_arg]

    log = []

    @dataclass
    class NotSet(Exception):
        name: str = ""

    @dataclass
    class NotFound(Exception):
        name: str = ""
        path: str = ""

    class manage:
        a_command_exists = False

        def __init__(self, title):
            self.title = title

        def __enter__(self):
            pass

        def __exit__(self, exc, val, trb) -> bool:
            if exc == NotSet:
                print(f"    - {self.title}: {val.name} not set")
            elif exc == NotFound:
                print(f"    - {self.title}: {val.name} not found")
                print(f"        for path {val.path}")
            elif isinstance(val, ImportError):
                print(f"    - {self.title}: dependency not installed")
                print(f"        {val}")
            else:
                manage.a_command_exists = True
                if exc:
                    print(f"    - {self.title}: conversion failed")
                    print(f"        {val}")

            return True

    with manage("cairosvg"):
        # CairoSVG
        import cairosvg

        with open(svg_path) as f:
            cairosvg.svg2png(
                file_obj=f,
                write_to=out_path,
                output_width=width,
                output_height=height,
            )
            return

    with manage("svglib"):
        # svglib + reportlab
        from reportlab.graphics import renderPM
        from svglib.svglib import svg2rlg

        rlg = svg2rlg(svg_path)
        renderPM.drawToFile(rlg, out_path, fmt="PNG")
        return

    with manage("Inkscape"):
        # system inkscape
        inkscape_path = "inkscape"
        if not which(inkscape_path):
            raise NotFound("inkscape", inkscape_path)
        subprocess.run([inkscape_path, *inkscape_args], check=True)
        return

    with manage("rsvg-convert"):
        # system rsvg-convert
        rsvg_convert_path = "rsvg-convert"
        if not which(rsvg_convert_path):
            raise NotFound("rsvg-convert", rsvg_convert_path)
        subprocess.run([rsvg_convert_path, *rsvg_convert_args], check=True)
        return

    with manage("rsvg-convert"):
        # system rsvg-convert - older versions?
        rsvg_convert_path = "rsvg"
        if not which(rsvg_convert_path):
            raise NotFound("rsvg-convert", rsvg_convert_path)
        subprocess.run([rsvg_convert_path, *rsvg_convert_args], check=True)
        return

    with manage("svgexport"):
        # system svgexport
        svgexport_path = "svgexport"
        if not which(svgexport_path):
            raise NotFound("svgexport", svgexport_path)
        subprocess.run([svgexport_path, *svgexport_args], check=True)
        return

    with manage("imagemagick"):
        # system imagemagick v7
        imagemagick_path = "magick"
        if not which(imagemagick_path):
            raise NotFound("imagemagick", imagemagick_path)
        subprocess.run([imagemagick_path, *imagemagick_args], check=True)
        return

    with manage("imagemagick"):
        # system imagemagick v7
        imagemagick_path = "convert"
        found_imagemagick_path = which(imagemagick_path)
        if (
            not found_imagemagick_path
            or "windows\\system32\\convert.exe" in found_imagemagick_path.lower()
        ):
            raise NotFound("imagemagick", imagemagick_path)
        subprocess.run([imagemagick_path, *imagemagick_args], check=True)
        return

    err_msg = ["\nFailed to convert svg to png."]

    if not manage.a_command_exists:
        err_msg.append(
            "No conversion tool was found. Please install one of: cairosvg, svglib, inkscape, rsvg-convert, svgexport, or imagemagick. If you have one of these installed, they may not be on your path. Pass your installed tool's path into this function."
        )

        raise NoToolError("\n".join(err_msg))

    raise FailedConversionError("\n".join(err_msg + log))

def svg_to_mono(
    svg_path: Union[str, Path],
    width: Optional[int] = None,
    height: Optional[int] = None,
):
    """
    Converts a svg to red/white and black/white 1bit bitmap images
    """

    svg_path = str(svg_path)
    png_out_path = f'{os.path.splitext(svg_path)[0]}.png'
    rw_out_path = f'{os.path.splitext(svg_path)[0]}.r.bmp'
    bw_out_path = f'{os.path.splitext(svg_path)[0]}.b.bmp'

    if width is None and height is None:
        width = 200
        height = 200

    # convert svg to png
    svg_to_png(svg_path, png_out_path, 200, 200)

    # open the png from file
    with Image.open(png_out_path) as input_image:
        # remove possible transparency
        with Image.new('RGB', input_image.size, (255,255,255)) as input_image_noalpha:
            input_image_noalpha.paste(input_image, mask=input_image.split()[3])
            # create palette image for black/red/white
            img_palette_brw = Image.new('RGB', (3,1), (255,255,255))
            img_palette_brw.putpixel((0,0), (0,0,0))
            img_palette_brw.putpixel((1,0), (255,0,0))
            img_palette_brw = img_palette_brw.convert('P',
                                                    palette = Image.Palette.ADAPTIVE,
                                                    dither=Image.Dither.NONE, colors=3)

            # convert input image to black/red/white with Floyd-Steinberg dither
            image_dithered_brw = input_image_noalpha.quantize(3,
                                                            dither=Image.Dither.FLOYDSTEINBERG,
                                                            palette = img_palette_brw)
            # convert black/red/white image to red/white
            # first modify palette from [white, red, black] to [white, red, white]
            image_rw = image_dithered_brw.copy()
            image_rw.putpalette([255,255,255,255,0,0,255,255,255])
            # then convert to binary image...
            # via RGB-image, adaptive quantization and finally conversion to mono
            image_rw = image_rw.convert('RGB')
            image_rw = image_rw.convert('P',
                                        palette = Image.Palette.ADAPTIVE,
                                        dither=Image.Dither.NONE, colors=2)
            image_rw = image_rw.convert('1')
            # write the image to file
            image_rw.save(rw_out_path, 'BMP')

            # convert black/red/white image to black/white
            # first modify palette from [white, red, black] to [white, white, black]
            image_bw = image_dithered_brw.copy()
            image_bw.putpalette([255, 255, 255, 255, 255, 255, 0, 0, 0])
            # then convert to binary image...
            # via RGB-image, adaptive quantization and finally conversion to mono
            image_bw = image_bw.convert('RGB')
            image_bw = image_bw.convert('P',
                                        palette = Image.Palette.ADAPTIVE,
                                        dither=Image.Dither.NONE, colors=2)
            image_bw = image_bw.convert('1')
            # write the image to file
            image_bw.save(bw_out_path, 'BMP')

    # delete temporary png
    os.remove(png_out_path)

    return

# input_filename = 'img_test.svg'
# input_path = './assets'

# svg_to_mono(os.path.join(input_path, input_filename))


os.makedirs('.pio/assets/fs_images', exist_ok=True)
for filename in ['img_door_open.svg', 'img_locked.svg', 'img_logo.svg', 'img_test.svg', 'img_unlocked.svg']:
    skip = False
    if os.path.isfile('.pio/assets/fs_images/' + filename + '.timestamp'):
        with open('.pio/assets/fs_images/' + filename + '.timestamp', 'r', -1, 'utf-8') as timestampFile:
            if os.path.getmtime('assets/fs_images/' + filename) == float(timestampFile.readline()):
                skip = True
    if skip:
        sys.stderr.write(f"svg2rbmono.py: {filename} up to date\n")
        continue
    sys.stderr.write(f"svg2rbmono.py: convert \'assets/fs_images/{filename}\' to \'data/{filename}.gz\'\n")
    copy('assets/fs_images/' + filename, 'data/' + filename)
    svg_to_mono('data/' + filename)
    with open('.pio/assets/fs_images/' + filename + '.timestamp', 'w', -1, 'utf-8') as timestampFile:
        timestampFile.write(str(os.path.getmtime('assets/fs_images/' + filename)))