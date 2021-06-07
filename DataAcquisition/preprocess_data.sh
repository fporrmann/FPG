cd multielectrode_grasp
git submodule update --init
cd ..
mkdir data
cd data
mkdir i140703-001
cd i140703-001
wget https://gin.g-node.org/INT/multielectrode_grasp/raw/master/datasets/i140703-001-03.nev
wget https://gin.g-node.org/INT/multielectrode_grasp/raw/master/datasets/i140703-001.odml
cd ../..
python generate_concatenated_data.py
python create_fpg_input.py
python occurrences_estimation.py
