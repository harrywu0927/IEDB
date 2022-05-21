# Novelty and Outlier Detection

import numpy as np
from sklearn.neighbors import LocalOutlierFactor
import joblib


def Outliers(lst, dimension):  # 对输入的一组点集寻找离群点，输出列表中-1表示为离群点
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


def Novelty(lst, dimension, valName):
    if(len(lst) % dimension != 0):
        return
    clf = joblib.load(valName.decode()+'_novelty_model.pkl')
    points = np.array(lst)
    points = points.reshape((len(points)/dimension), dimension)
    res = clf.predict(points)
    return list(res)


def NoveltyModelTrain(lst, dimension, valName):
    points = np.array(lst)
    neighbors = 20
    if(len(points) < 50):
        neighbors = len(points)/10
    if len(lst) % dimension == 0:
        rows = len(lst)/dimension
        points = points.reshape(int(rows), dimension)
        clf = LocalOutlierFactor(novelty=True, n_neighbors=neighbors)
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
    rang = np.where(y > 0)
    normal = points[rang[0]]
    minline = np.min(normal)
    maxline = np.max(normal)
    print(float(maxline), float(minline))
    return float(maxline), float(minline)
