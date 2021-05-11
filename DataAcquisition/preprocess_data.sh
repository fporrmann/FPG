cd multielectrode_grasp/datasets
gin get-content i140703-001-03.nev
gin get-content i140703-001.odml
cd ../..
python generate_concatenated_data.py
python create_fpg_input.py
python occurrences_estimation.py