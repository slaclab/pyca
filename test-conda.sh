mkdir -p conda-bld-tst
conda build conda-recipe --output-folder conda-bld-tst --python 2.7 --numpy 1.13
