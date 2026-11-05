"""Auto-imported by every Python started with dev/pyshim on PYTHONPATH.

Windows PyPI Pillow wheels ship without libraqm, so FreeType cannot apply
OpenType features and any call passing `features=` raises KeyError. The only
feature Felucca's generators ask for is `tnum` (tabular figures), and Inter
Tight's figures are tabular by default, so ignoring it changes no output.
Felucca's generate() runs each generator as a subprocess, which is why this
lives in a sitecustomize rather than in dev/gen.py.
"""
import warnings

try:
    from PIL import ImageFont, features
except Exception:                                  # no Pillow here: nothing to do
    pass
else:
    if not features.check("raqm"):
        warnings.filterwarnings("ignore", message="Raqm layout was requested")

        # direction / features / language are positions 2-4 of getmask2 and
        # ImageDraw passes them positionally, so blank them wherever they sit.
        def _drop_features(fn):
            def wrapper(self, *a, **kw):
                try:
                    return fn(self, *a, **kw)
                except KeyError as e:
                    if "libraqm" not in str(e):
                        raise
                    a = list(a)
                    for _i in (2, 3, 4):
                        if len(a) > _i:
                            a[_i] = None
                    for _k in ("direction", "features", "language"):
                        kw.pop(_k, None)
                    return fn(self, *a, **kw)
            return wrapper

        for _name in ("getlength", "getmask", "getmask2", "getbbox"):
            if hasattr(ImageFont.FreeTypeFont, _name):
                setattr(ImageFont.FreeTypeFont, _name,
                        _drop_features(getattr(ImageFont.FreeTypeFont, _name)))
