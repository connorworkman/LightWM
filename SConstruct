import os

ENVIRONMENTS = ('gcc')
env = Environment()
env.Append(CXXFLAGS=['-std=c++1y', '-Wall', '-g',], ENV={'PATH': os.environ.get('PATH', '')})
LIBS=['x11',]
for lib in LIBS:
    env.ParseConfig('pkg-config --cflags --libs %s' % (lib))
env.Program('lightwm', Glob('*.cpp'))
