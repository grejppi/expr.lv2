#!/usr/bin/env python
import os
import shutil
import waflib.extras.autowaf as autowaf

APPNAME = 'lv2-expr'
VERSION = '0.0.1'
top = '.'
out = 'build'

BUNDLE = APPNAME + '.lv2'

def options(opt):
    opt.load('compiler_c')
    opt.load('lv2')
    autowaf.set_options(opt)

def configure(conf):
    conf.load('compiler_c')
    conf.load('lv2')
    autowaf.configure(conf)
    autowaf.set_c99_mode(conf)
    autowaf.check_pkg(conf, 'lv2', atleast_version='1.10.0', uselib_store='LV2')

    # Set env.pluginlib_PATTERN
    pat = conf.env.cshlib_PATTERN
    if pat[0:3] == 'lib':
        pat = pat[3:]
    conf.env.pluginlib_PATTERN = pat
    conf.env.pluginlib_EXT = pat[pat.rfind('.'):]

def build_plugin(bld, bundle, name, source, defines=None):
    # Build plugin library
    penv = bld.env.derive()
    penv.cshlib_PATTERN = bld.env.pluginlib_PATTERN
    obj = bld(features     = 'c cshlib',
              env          = penv,
              source       = source,
              includes     = ['.'],
              name         = name,
              target       = '%s/%s' % (bundle, name),
              uselib       = ['LV2'],
              install_path = '%s/%s' % ('${LV2DIR}', BUNDLE))
    if defines != None:
        obj.defines = defines

def build(bld):
    def do_copy(task):
        return shutil.copy(
            task.inputs[0].abspath(),
            task.outputs[0].abspath())

    for i in bld.path.ant_glob('%s/%s' % (BUNDLE, '*.ttl')):
        bld(features     = 'subst',
            is_copy      = True,
            source       = i,
            target       = bld.path.get_bld().make_node('%s/%s' % (BUNDLE, i)),
            install_path = '%s/%s' % ('${LV2DIR}', BUNDLE))

    bld(features     = 'subst',
        source       = '%s/%s' % (BUNDLE, 'manifest.ttl.in'),
        target       = bld.path.get_bld().make_node('%s/%s' % (BUNDLE, 'manifest.ttl')),
        LIB_EXT      = bld.env.pluginlib_EXT,
        install_path = '%s/%s' % ('${LV2DIR}', BUNDLE))

    plugins = ['randomtuner', 'exprsynth']
    for i in plugins:
        build_plugin(bld, BUNDLE, i, ['src/%s.c' % i])
