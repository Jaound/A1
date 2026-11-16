"""Plot saved experiments with sample sizes; no simulations."""
import json
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

R=Path(__file__).parent;r=json.loads((R/'results.json').read_text());ss=r['summary']
fig,axes=plt.subplots(1,3,figsize=(15,4.5),layout='constrained')
ax=axes[0];ax.axhspan(10,15,color='#dcefe2',label='Design target: 10–15 min')
for i,b in enumerate((100,200,300)):
    xs=[x['seconds']/60 for x in r['trials'] if x['case']==f'fleet_mixed_{b}']
    q=np.quantile(xs,[.25,.5,.75]);ax.errorbar(i,q[1],yerr=[[q[1]-q[0]],[q[2]-q[1]]],fmt='o',color='#247595',capsize=5)
    ax.text(i,q[2]+.5,f'n={len(xs)}',ha='center')
ax.set_xticks(range(3),['100','200','300']);ax.set(xlabel='Fleet cost',ylabel='Game minutes',title='Mixed fleets: final end, median / IQR',ylim=(9.5,22));ax.legend(fontsize=8)
ax=axes[1];labels=[];arrived=[];returned=[]
for method in ('pod','assault'):
    for policy in ('off','until_exit'):
        s=ss[f'linked_100_{method}_{policy}'];labels.append(method+'\n'+policy);arrived.append(s['mean_entered_heroes']);returned.append(s['mean_returned_heroes'])
x=np.arange(4);ax.bar(x-.18,arrived,.36,label='Entered');ax.bar(x+.18,returned,.36,label='Returned')
ax.set_xticks(x,labels,fontsize=8);ax.set(ylabel='Mean heroes / 4 launched',ylim=(0,4.4),title='100 cost / L10 boarding, n=8 per case');ax.legend(fontsize=8)
ax=axes[2];wins=[];err=[]
for lv in (1,10,20):
    s=ss[f'indoor_4_4_L{lv}'];n=s['n'];p=s['left_wins']/n;z=1.96;den=1+z*z/n
    mid=(p+z*z/(2*n))/den;half=z*((p*(1-p)/n+z*z/(4*n*n))**.5)/den
    wins.append(p*100);err.append((max(0,(p-(mid-half))*100),max(0,((mid+half)-p)*100)))
ax.bar(range(3),wins,color='#247595');ax.errorbar(range(3),wins,yerr=np.array(err).T,fmt='none',color='#222',capsize=5)
ax.set_xticks(range(3),['L1','L10','L20']);ax.set(ylim=(0,105),ylabel='Attacker wins (%)',title='Indoor 4 vs 4, n=16 / level, Wilson 95%')
fig.suptitle('Orbital Vanguard — D116 v0.6 design model (not UE gameplay)',fontsize=13)
fig.savefig(R/'results.png',dpi=160);fig.savefig(R/'results.svg')
svg=R/'results.svg';svg.write_text('\n'.join(line.rstrip() for line in svg.read_text().splitlines())+'\n')
print('Saved results.png / results.svg')
