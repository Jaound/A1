import json,hashlib
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
R=Path(__file__).parent
r=json.loads((R/'results.json').read_text());assert r['profile_sha256']==hashlib.sha256((R/'profile.json').read_bytes()).hexdigest();s=r['summary'];budgets=(100,200,300)
fig,axes=plt.subplots(3,1,figsize=(10,11),layout='constrained')
x=range(3);ax=axes[0];ax.axhspan(10,15,color='green',alpha=.12,label='Requested 10–15 min')
for offset,version,label,color in [(-.16,'old','Old hull/shield only','#a8a8a8'),(.16,'new','New mixed fleet','#247ba0')]:
 ax.bar([i+offset for i in x],[s[f'{b}_mixed_mixed_none_{version}_screen']['median_seconds']/60 for b in budgets],width=.32,label=label,color=color)
ax.set(xticks=list(x),xticklabels=budgets,ylabel='Game minutes',title='Median full-battle duration at equal fleet cost');ax.legend(fontsize=8)
ax=axes[1]
for i,(kind,label) in enumerate([('heavy','Battleship stack'),('light','Small stack'),('carrier','Carrier stack'),('xl','XL stack')]):
 ax.bar([v+(i-1.5)*.18 for v in x],[100*s[f'{b}_mixed_{kind}_none_new_screen']['wins']/24 if not (b==100 and kind=='xl') else 0 for b in budgets],width=.17,label=label)
ax.set(xticks=list(x),xticklabels=budgets,ylabel='Mixed fleet wins (%)',ylim=(0,110),title='Composition stress tests (24 seeds each; 100-cost XL is unavailable)');ax.legend(fontsize=8,ncol=2)
ax=axes[2]
for offset,policy,label,color in [(-.18,'weapon','Assault transport → weapon room','#247ba0'),(.18,'pod','Direct pod → weapon room','#70ae6e')]:
 ax.bar([i+offset for i in x],[s[f'{b}_mixed_mixed_{policy}_new_screen']['mean_entered']/4*100 for b in budgets],width=.35,label=label,color=color)
ax.set(xticks=list(x),xticklabels=budgets,xlabel='Fleet cost cap',ylabel='Heroes entering (%)',ylim=(0,105),title='Boarding entry without holding fleet fire (not final survival)');ax.legend(fontsize=8)
fig.suptitle('Orbital Vanguard endurance v0.3c — aggregate model, not UE gameplay',fontsize=12)
fig.savefig(R/'results.png',dpi=150);fig.savefig(R/'results.svg');plt.close(fig)

svg=R/'results.svg';svg.write_text('\n'.join(line.rstrip() for line in svg.read_text().splitlines())+'\n')
