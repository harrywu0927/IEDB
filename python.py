import numpy as np
import pandas
# import matplotlib
# import matplotlib.pyplot as plt
from sklearn.neighbors import LocalOutlierFactor


def multiply(a, b):
    print('will muitiply '+str(a)+' and '+str(b))
    return a*b
# print(multiply(2,3))


def LOF():
    xx, yy = np.meshgrid(np.linspace(-5, 5, 500), np.linspace(-5, 5, 500))
    # 生成正常（非异常）的训练观察结果
    X = 0.3 * np.random.randn(100, 2)
    X_train = np.r_[X + 2, X - 2]
    # 生成新的正常（非异常）观测值
    X = 0.3 * np.random.randn(20, 2)
    X_test = np.r_[X + 2, X - 2]
    # 生成新的异常的观测值
    X_outliers = np.random.uniform(low=-4, high=4, size=(20, 2))

    # 拟合模型以进行新颖性检测（novelty = True）
    clf = LocalOutlierFactor(n_neighbors=20, novelty=True, contamination=0.1)
    clf.fit(X_train)
    # 请勿在X_train上使用predict，decision_function和score_samples，因为这样会产生错误的结果，而只会在新的、没见过的数据上使用（X_train中未使用），例如X_test，X_outliers或meshgrid
    y_pred_test = clf.predict(X_test)
    y_pred_outliers = clf.predict(X_outliers)
    n_error_test = y_pred_test[y_pred_test == -1].size
    n_error_outliers = y_pred_outliers[y_pred_outliers == 1].size
    print(y_pred_test)
    return list(y_pred_test)
    # 绘制学习的边界，点和最接近平面的向量
    Z = clf.decision_function(np.c_[xx.ravel(), yy.ravel()])
    Z = Z.reshape(xx.shape)

    plt.title("Novelty Detection with LOF")
    plt.contourf(xx, yy, Z, levels=np.linspace(
        Z.min(), 0, 7), cmap=plt.cm.PuBu)
    a = plt.contour(xx, yy, Z, levels=[0], linewidths=2, colors='darkred')
    plt.contourf(xx, yy, Z, levels=[0, Z.max()], colors='palevioletred')

    s = 40
    b1 = plt.scatter(X_train[:, 0], X_train[:, 1],
                     c='white', s=s, edgecolors='k')
    b2 = plt.scatter(X_test[:, 0], X_test[:, 1], c='blueviolet', s=s,
                     edgecolors='k')
    c = plt.scatter(X_outliers[:, 0], X_outliers[:, 1], c='gold', s=s,
                    edgecolors='k')
    plt.axis('tight')
    plt.xlim((-5, 5))
    plt.ylim((-5, 5))
    plt.legend([a.collections[0], b1, b2, c],
               ["learned frontier", "training observations",
                "new regular observations", "new abnormal observations"],
               loc="upper left",
               prop=matplotlib.font_manager.FontProperties(size=11))
    plt.xlabel(
        "errors novel regular: %d/40 ; errors novel abnormal: %d/40"
        % (n_error_test, n_error_outliers))
    plt.show()


LOF()
