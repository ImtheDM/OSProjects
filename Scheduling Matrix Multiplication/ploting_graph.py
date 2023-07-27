import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# Read the data
def reading(number_of_threads):
    df = pd.read_csv('MyFile_read.csv')
    df.drop_duplicates
    a=[]
    i=0
    while i<df.shape[0]:
        a.append(i)
        i=i+number_of_threads
    dic={}
    for i in range(0,number_of_threads):
        li=[]
        for i in range(0,len(a)):
            li.append(df['Nanoseconds'].iloc[a[i]])
        dic[a[0]]=li
        for i in range(0,len(a)):
            a[i]=a[i]+1
    for i in dic:
        dic[i]=np.mean(dic[i])
    print(dic)
    df = pd.DataFrame.from_dict(dic, orient='index')
    df.to_csv('fr4.csv', header=False)
    # Plot the data
    plt.plot(dic.keys(), dic.values(), 'r')
    plt.xlabel('Number of threads')
    plt.ylabel('Nanoseconds')
    plt.title('Reading')
    plt.show()

def writing(number_of_threads):
    df = pd.read_csv('MyFile_write.csv')
    df.drop_duplicates
    a=[]
    i=0
    while i<df.shape[0]:
        a.append(i)
        i=i+number_of_threads
    dic={}
    for i in range(0,number_of_threads):
        li=[]
        for i in range(0,len(a)):
            li.append(df['Nanoseconds'].iloc[a[i]])
        dic[a[0]]=li
        for i in range(0,len(a)):
            a[i]=a[i]+1
    for i in dic:
        dic[i]=np.mean(dic[i])
    print(dic)
    df = pd.DataFrame.from_dict(dic, orient='index')
    df.to_csv('fw4.csv', header=False)
    # Plot the data
    plt.plot(dic.keys(), dic.values(), 'r')
    plt.xlabel('Number of threads')
    plt.ylabel('Nanoseconds')
    plt.title('Writing')
    plt.show()

def pplot():
    df1_r=pd.read_csv('100_100_100_r.csv')
    df2_r=pd.read_csv('777_888_999_r.csv')
    # df3_r=pd.read_csv('987_876_765_r.csv')
    df4_r=pd.read_csv('1000_500_1000_r.csv')
    plt.plot(df4_r['Number of thread'], df4_r['Nanoseconds'], 'r', label='1000_500_1000_r')
    plt.xlabel('Number of threads')
    plt.ylabel('Nanoseconds')
    plt.title('Reading')
    plt.legend()
    plt.show()
    
    # plot the data
    plt.plot(df1_r['Number of thread'], df1_r['Nanoseconds'], 'r', label='100_100_100_r')
    plt.plot(df2_r['Number of thread'], df2_r['Nanoseconds'], 'b', label='777_888_999_r')
    # plt.plot(df3_r['Number of thread'], df3_r['Nanoseconds'], 'g', label='987_876_765_r')
    plt.plot(df4_r['Number of thread'], df4_r['Nanoseconds'], 'b', label='1000_500_1000_r')
    plt.xlabel('Number of threads')
    plt.ylabel('Nanoseconds')
    plt.title('Reading')
    plt.legend()
    plt.show()

    df1_w=pd.read_csv('100_100_100_w.csv')
    df2_w=pd.read_csv('777_888_999_w.csv')
    # df3_w=pd.read_csv('987_876_765_w.csv')
    df4_w=pd.read_csv('1000_500_1000_w.csv')
    plt.plot(df4_w['Number of thread'], df4_w['Nanoseconds'], 'r', label='1000_500_1000_r')
    plt.xlabel('Number of threads')
    plt.ylabel('Nanoseconds')
    plt.title('Writing')
    plt.legend()
    plt.show()
    # plot the data
    plt.plot(df1_w['Number of thread'], df1_w['Nanoseconds'], 'r', label='100_100_100_w')
    plt.plot(df2_w['Number of thread'], df2_w['Nanoseconds'], 'b', label='777_888_999_w')
    # plt.plot(df3_w['Number of thread'], df3_w['Nanoseconds'], 'g', label='987_876_765_w')
    plt.plot(df4_w['Number of thread'], df4_w['Nanoseconds'], 'b', label='1000_500_1000_w')
    plt.xlabel('Number of threads')
    plt.ylabel('Nanoseconds')
    plt.title('Writing')
    plt.legend()
    plt.show()
    

def plott():
    df=pd.read_csv('context-switch-time-2ms.csv')
    df1=pd.read_csv('p2-waiting-time-2ms.csv')
    a=[]
    i=0
    while i<df.shape[0]:
        a.append(i)
        i=i+10
    dic={}
    for i in range(0,10):
        li=[]
        for i in range(0,len(a)):
            li.append(df['Avg Context Switch Time'].iloc[a[i]])
        dic[a[0]]=li
        for i in range(0,len(a)):
            a[i]=a[i]+1
    for i in dic:
        dic[i]=np.mean(dic[i])
    print(dic)
    df = pd.DataFrame.from_dict(dic, orient='index')
    # Plot the data
    plt.plot([50,100,150,200,250,300,350,400,450,500], dic.values(), 'r')
    plt.xlabel('Matrix Size')
    plt.ylabel('Nanoseconds')

    plt.show()

    a=[]
    i=0
    while i<df1.shape[0]:
        a.append(i)
        i=i+10
    dic={}
    for i in range(0,10):
        li=[]
        for i in range(0,len(a)):
            li.append(df1['Total Waiting Time'].iloc[a[i]])
        dic[a[0]]=li
        for i in range(0,len(a)):
            a[i]=a[i]+1
    for i in dic:
        dic[i]=np.mean(dic[i])
    print(dic)
    df1 = pd.DataFrame.from_dict(dic, orient='index')
    # Plot the data
    plt.plot([50,100,150,200,250,300,350,400,450,500], dic.values(), 'r')
    plt.xlabel('Matrix Size')
    plt.ylabel('Nanoseconds')
    plt.title('p2')
    plt.show()

plott()