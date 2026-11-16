"""Optional export; requires matplotlib, unlike the simulation itself."""
import json
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
ROOT=Path(__file__).parent
r=json.loads((ROOT/'results.json').read_text());summary=r['summary']
fig,axs=plt.subplots(3,1,figsize=(11,11),layout='constrained')
for ax,domain,title in zip(axs[:2],('fleet','hero'),('Fleet combat: left-side wins (95% Wilson interval)','Indoor combat: left-side wins (95% Wilson interval)')):
 keys=[k for k,v in summary.items() if v['domain']==domain];wins=[summary[k]['left_win_percent'] for k in keys];lo=[wins[i]-summary[k]['left_win_wilson95_percent'][0] for i,k in enumerate(keys)];hi=[summary[k]['left_win_wilson95_percent'][1]-wins[i] for i,k in enumerate(keys)]
 ax.bar(keys,wins,color='#247ba0');ax.errorbar(keys,wins,yerr=[lo,hi],fmt='none',color='#1f2937',capsize=3);ax.set(ylim=(0,105),ylabel='Percent',title=title);ax.grid(axis='y',alpha=.2);ax.set_axisbelow(True)
keys=[k for k,v in summary.items() if v['domain']=='hybrid' and k not in ('M01','M02')];x=list(range(len(keys)));ax=axs[2]
for offset,key,label,color,scale in [(-.25,'mean_objective','Objective achieved','#247ba0',100),(0,'mean_heroes_extracted','Returned heroes alive at end / 4','#70ae6e',25),(.25,'mean_heroes_dead','Heroes permanently lost / 4','#d55e00',25)]:
 ax.bar([i+offset for i in x],[summary[k][key]*scale for k in keys],width=.24,label=label,color=color)
ax.set(xticks=x,xticklabels=keys,ylim=(0,105),ylabel='Percent',title='Fleet + boarding: success, return and permanent loss are different outcomes');ax.grid(axis='y',alpha=.2);ax.set_axisbelow(True);ax.legend(loc='upper left',fontsize=8)
fig.suptitle('Orbital Vanguard combat-v0.2 — reduced Python model, not Unreal gameplay',fontsize=13)
fig.savefig(ROOT/'combat_results.png',dpi=160);fig.savefig(ROOT/'combat_results.svg');plt.close(fig)
