base_path=~/Downloads/vosk-model-ru-0.22-compile/exp/chain/tdnn
model=$base_path/final.mdl
tree=$base_path/tree

dir=./prepared_model
mkdir $dir

grep '#' $base_path/graph/phones.txt | awk '{print $2}' > $dir/disambig_phones.list
subseq_sym=`tail -1 $base_path/graph/phones.txt | awk '{print $2+1;}'`
fstmakecontextfst --read-disambig-syms=$dir/disambig_phones.list --write-disambig-syms=$dir/disambig_ilabels.list --context-size=2 --central-position=1 $base_path/graph/phones.txt $subseq_sym $dir/ilabels | fstarcsort --sort_type=ilabel > $dir/C.fst
#fstaddsubsequentialloop $subseq_sym $dir/C.fst > $dir/C2.fst
make-ilabel-transducer --write-disambig-syms=$dir/disambig_ilabels_remapped.list $dir/ilabels $tree $model $dir/ilabels.remapped > $dir/ilabel_map.fst
fstcompose $dir/ilabel_map.fst $dir/C.fst | fstdeterminizestar --use-log=true | fstminimizeencoded > $dir/C2.fst
make-h-transducer --disambig-syms-out=$dir/disambig_tstate.list --transition-scale=1.0  $dir/ilabels.remapped $tree $model > $dir/Ha.fst
fsttablecompose $dir/Ha.fst $dir/C2.fst | fstdeterminizestar --use-log=true | fstrmsymbols $dir/disambig_tstate.list | fstrmepslocal  | fstminimizeencoded > $dir/HCa.fst
add-self-loops --self-loop-scale=1.0 --reorder=true $model $dir/HCa.fst | fstrmsymbols --remove-arcs --remove-from-output $dir/disambig_phones.list | fstconvert --fst_type=const > $dir/HC.fst
cp $model $dir/
