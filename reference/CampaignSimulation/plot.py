#!/usr/bin/env python3
"""Optional standalone plot; requires matplotlib. Simulation uses stdlib only."""
from pathlib import Path
import csv,json
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
out=Path(__file__).resolve().parent
data=json.loads((out/'results_ten.json').read_text())
rows=list(csv.DictReader((out/'trace_ten.csv').open()));t=[float(x['real_min']) for x in rows]
fig,axes=plt.subplots(2,1,figsize=(11,7.5),sharex=True,layout='constrained')
axes[0].plot(t,[float(x['power_proxy']) for x in rows],color='#167ba6',lw=2,label='Force proxy (not simulated combat strength)')
ax=axes[0].twinx();ax.step(t,[float(x['missions']) for x in rows],where='post',color='#c57816',alpha=.85,label='Completed mission workload');ax.set_ylim(0,10.7);ax.set_ylabel('Mission count')
for key,label in [('first_destroyer','First medium'),('first_battleship','First large'),('first_dreadnought','First capital')]:
 x=data['representative']['milestones'][key]['real'];axes[0].axvline(x,color='#667788',ls=':',alpha=.65);axes[0].text(x+2,axes[0].get_ylim()[1]*.82,label,rotation=90,va='top',fontsize=9)
axes[0].set_ylabel('Force points');axes[0].legend(loc='upper left',fontsize=8);ax.legend(loc='upper right',fontsize=8)
for field,label,color in [('general','General','#3178ac'),('advanced','Advanced','#bd6d14'),('rare','Rare','#8845b5')]:axes[1].plot(t,[float(x[field]) for x in rows],label=label,color=color)
axes[1].set_ylabel('Available resource stock');axes[1].set_xlabel('Real elapsed minutes (including assumed pauses)');axes[1].legend(loc='upper left')
for a in axes:a.grid(alpha=.2);a.set_xlim(0,300)
fig.suptitle('Illustrative 10-mission economy/queue model | mixed speed, standard loss, seed 0\nNo UE combat, hero progression or 3D layout simulation',fontsize=11)
fig.savefig(out/'campaign_timeline.png',dpi=150);plt.close(fig)
