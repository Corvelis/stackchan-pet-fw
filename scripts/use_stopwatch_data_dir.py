from os.path import join

Import("env")

env.Replace(PROJECT_DATA_DIR=join(env.subst("$PROJECT_DIR"), "data_stopwatch"))
