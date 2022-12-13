import numpy as np
import pandas as pd

def diff_of_two_list(list1,list2):
    return [x for x in list1 if x not in list2]

#Z-score方法
def detect_outliers(data,threshold=1):
    mean_d = np.mean(data)  #平均值
    std_d = np.std(data)    #标准差
    outliers = []
    for y in data:
        z_score= (y - mean_d)/std_d
        if np.abs(z_score) > threshold:
            outliers.append(y)
    return outliers



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
    test = detect_outliers(testData)
    print("非正常值：",test)
    normal = diff_of_two_list(testData,test)
    print("正常值：",normal)
    print("最大值：",max(normal))
    print("最小值：",min(normal))
    print("平均值：",int(np.mean(normal)))