# Novelty and Outlier Detection

import numpy as np
from sklearn.neighbors import LocalOutlierFactor
import joblib


def Outliers(lst, dimension):
    points = np.array(lst)
    neighbors = 20
    if (len(points) < 50):
        neighbors = 5
    if len(lst) % dimension == 0:
        rows = len(lst)/dimension
        points = points.reshape(int(rows), dimension)
        print(points)
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
    if (len(points) < 50):
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
    y = Outliers(lst, 1)
    y = np.array(y)
    print(y)
    rang = np.where(y > 0)
    normal = points[rang[0]]
    minline = np.min(normal)
    maxline = np.max(normal)
    print(maxline, minline)
    return float(maxline), float(minline)


def NoveltyFit_new(lst):
    points = np.array(lst)
    rows = len(lst)
    points = points.reshape(int(rows), 1)
    y = Outliers(lst, 1)
    y = np.array(y)
    print(y)
    rang = np.where(y > 0)
    normal = points[rang[0]]
    minline = np.min(normal)
    maxline = np.max(normal)
    avgLine = np.mean(normal)
    print(maxline, minline, avgLine)
    return float(maxline), float(minline), float(avgLine)


def NoveltyFit_JF(lst):
    points = np.array(lst)
    rows = len(lst)/2
    points = points.reshape(int(rows), 2)
    points = np.array(lst)
    # print(points)
    clf = LocalOutlierFactor(n_neighbors=2000)
    y = clf.fit_predict(points)
    y = np.array(y)
    print(y)
    rang = np.where(y > 0)
    normal = points[rang[0]]
    minline = np.min(normal)
    maxline = np.max(normal)
    print(maxline, minline)
    return float(maxline), float(minline)

testData = [217,217,216,220,215,210,215,216,216,216,218,212,200,218,216,216,230,
211,217,216,220,215,210,215,216,216,216,218,212,200,218,216,216,230,
212,217,216,220,215,210,215,216,216,216,218,212,200,218,216,216,230,
213,217,216,220,215,210,215,216,216,216,218,212,200,218,216,216,230,
214,217,216,220,215,210,215,216,216,216,218,212,200,218,216,216,230,
215,217,216,220,215,210,215,216,216,216,218,212,200,218,216,216,230,
216,217,216,220,215,210,215,216,216,216,218,212,200,218,216,216,230,
211,217,216,220,215,210,215,216,216,216,218,212,200,218,216,216,230,
212,217,216,220,215,210,215,216,216,216,218,212,200,218,216,216,230,
213,217,216,220,215,210,215,216,216,216,218,212,200,218,216,216,230]

if __name__ == "__main__":
    NoveltyFit_new(testData)