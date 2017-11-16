mkdir -p conda-bld-tst
conda build conda-recipe --output-folder conda-bld-tst --python 3.6 --numpy 1.13
