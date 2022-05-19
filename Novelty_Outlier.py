# Novelty and Outlier Detection

import numpy as np
from sklearn.neighbors import LocalOutlierFactor


def Outliers(lst, dimension):
    points = np.array(lst)
    neighbors = 20
    if(len(points) < 50):
        neighbors = 5
    if len(lst) % dimension == 0:
        rows = len(lst)/dimension
        points = points.reshape(int(rows), dimension)
        # print(points)
        clf = LocalOutlierFactor(n_neighbors=neighbors)
        res = clf.fit_predict(points)
        return list(res)
    else:
        print('输入数组shape有误')
    return


def Novelty(lst, dimension):
    points = np.array(lst)
    if len(lst) % dimension == 0:
        rows = len(lst)/dimension
        points = points.reshape(int(rows), dimension)
        print(points)
        y = np.linspace(-20, 100, 2000).reshape(2000, 1)
        clf = LocalOutlierFactor(novelty=True)
        clf.fit(points)
        z = clf.decision_function(y)
        rang = np.where(z > 0)
        minline = -20 + 120/2000 * rang[0][0]
        maxline = -20 + 120/2000 * rang[0][len(rang[0])-1]
        return maxline, minline

    else:
        print('输入数组shape有误')
    return


def NoveltyFit(lst):
    points = np.array(lst)
    rows = len(lst)
    points = points.reshape(int(rows), 1)
    print(points)
    maxVal = np.max(points)
    minVal = np.min(points)
    print(maxVal, minVal)
    y = np.linspace(minVal, maxVal, np.round(
        (maxVal-minVal) * 100)).reshape((maxVal-minVal) * 100, 1)
    clf = LocalOutlierFactor(novelty=True)
    clf.fit(points)
    z = clf.decision_function(y)
    rang = np.where(z > 0)
    minline = y[rang[0][0]]
    maxline = y[rang[0][len(rang[0])-1]]
    print(maxline, minline)
    return maxline, minline
