7.17
=====
1. remove icu/tcmalloc/jemalloc
* gurl (idntoascii)
* delete icu,and comment i18n/.cc,then fix compile
2. unittests pass
* render_text.cc!!!
3. example pass

7.16
=====
1. views-->ui
2. unitests pass
3. example pass

7.16
=====
1. base_i18n-->icu
2. gurl-->base_i18n
3. ui-->skia+base_i18n+gurl
4. unitests pass

note:
enable_hidpi=0
enable_touch_ui=0
enable_gpu=0
enable_printing=0
chrome/tools/build/repack_locales.py

7.16
=====
1. base-->zip+gmock+gtest
2. skia-->base+png+jpeg (no-gpu no-pringting/pdf)
3. unitests pass

7.15
=====
1. orig: chromium 31.0.1650.57
2. + base
   + testing
   + build
   + jemalloc/modp_b64/tcmalloc/zlib
   - icu/libxml
3. unitests pass

gen project
======
run gen_proj.bat

commit changes
=====
git add -A
git commit -m "msg"
git pull --rebase
git push origin master