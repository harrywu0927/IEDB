import numpy as np


def MAX(arr):
    return list(np.max(arr, axis=0))


def STD(arr):
    return list(np.std(arr, axis=0))


def STDEV(arr):
    return list(np.var(arr, axis=0))


def SUM(arr):
    return list(np.sum(arr, axis=0))


def MIN(arr):
    return list(np.min(arr, axis=0))


def MEAN(arr):
    return list(np.mean(arr, axis=0))


def MEDIAN(arr):
    return list(np.median(arr, axis=0))


def PROD(arr):
    return list(np.prod(arr, axis=0))


# for test


def printValue(value):
    print(value)
