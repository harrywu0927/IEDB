# Novelty and Outlier Detection

#from ctypes.wintypes import FLOAT, LONG
import numpy as np
from sklearn.neighbors import LocalOutlierFactor
import joblib


def Outliers(lst, dimension):
    points = np.array(lst)
    neighbors = 20
    if(len(points) < 50):
        neighbors = 5
    if len(lst) % dimension == 0:
        rows = len(lst)/dimension
        points = points.reshape(int(rows), dimension)
        # print(points)
        clf = LocalOutlierFactor(n_neighbors=int(neighbors))
        res = clf.fit_predict(points)
        return list(res)
    else:
        print('输入数组shape有误')


"""_summary_
    """


def Novelty(vals, dimensions, names):
    res = []
    for i in range(len(vals)):
        print(names[i].decode())
        clf = joblib.load(names[i].decode()+'_novelty_model.pkl')
        points = np.array(vals[i])
        points = points.reshape((len(points)/dimensions[i]), dimensions[i])
        res.append(clf.predict(points))
    return list(res)


def Novelty_Single_Column(vals, dim, name):
    res = []
    print(name.decode())
    clf = joblib.load(name.decode()+'_novelty_model.pkl')
    points = np.array(vals)
    points = points.reshape(int(len(points)/dim), dim)
    res = clf.predict(points)
    print(res)
    return list(res)


def NoveltyModelTrain(lst, dimension, valName):
    points = np.array(lst)
    neighbors = 20
    if(len(points) < 50):
        neighbors = len(points)/5
    if len(lst) % dimension == 0:
        rows = len(lst)/dimension
        points = points.reshape(int(rows), dimension)
        clf = LocalOutlierFactor(novelty=True, n_neighbors=int(neighbors))
        clf.fit(points)
        joblib.dump(clf, valName.decode()+'_novelty_model.pkl')
        return 0
    else:
        print('输入数组shape有误')
        return -1


def NoveltyFit(lst):
    points = np.array(lst)
    rows = len(lst)
    points = points.reshape(int(rows), 1)
    print(points)
    y = Outliers(lst, 1)
    y = np.array(y)
    print(y)
    rang = np.where(y > 0)
    normal = points[rang[0]]
    minline = np.min(normal)
    maxline = np.max(normal)
    print(maxline, minline)
    return float(maxline), float(minline)
