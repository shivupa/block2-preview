
#include "quantum.hpp"
#include <gtest/gtest.h>

using namespace block2;

class TestDMRG : public ::testing::Test {
  protected:
    size_t isize = 1L << 30;
    size_t dsize = 1L << 34;
    void SetUp() override {
        Random::rand_seed(0);
        frame = new DataFrame(isize, dsize, "nodex");
    }
    void TearDown() override {
        frame->activate(0);
        assert(ialloc->used == 0 && dalloc->used == 0);
        delete frame;
    }
};

TEST_F(TestDMRG, Test) {
    shared_ptr<FCIDUMP> fcidump = make_shared<FCIDUMP>();
    vector<double> occs;
    string occ_filename = "data/CR2.SVP.OCC";
    occs = read_occ(occ_filename);
    string filename = "data/CR2.SVP.FCIDUMP"; // E = -2086.504520308260
    // string filename = "data/N2.STO3G.FCIDUMP"; // E = -107.65412235
    // string filename = "data/HUBBARD-L8.FCIDUMP"; // E = -6.22563376
    // string filename = "data/HUBBARD-L16.FCIDUMP"; // E = -12.96671541
    fcidump->read(filename);
    vector<uint8_t> orbsym = fcidump->orb_sym();
    transform(orbsym.begin(), orbsym.end(), orbsym.begin(),
              Hamiltonian<SU2>::swap_d2h);
    SU2 vaccum(0);
    SU2 target(fcidump->n_elec(), fcidump->twos(),
                     Hamiltonian<SU2>::swap_d2h(fcidump->isym()));
    int norb = fcidump->n_sites();
    bool su2 = !fcidump->uhf;
    Hamiltonian<SU2> hamil(vaccum, target, norb, orbsym, fcidump);

    // mkl_set_num_threads(4);
    // mkl_set_dynamic(0);

    Timer t;
    t.get_time();
    // MPO construction
    cout << "MPO start" << endl;
    shared_ptr<MPO<SU2>> mpo = make_shared<MPOQC<SU2>>(hamil, QCTypes::Conventional);
    cout << "MPO end .. T = " << t.get_time() << endl;

    // MPO simplification
    cout << "MPO simplification start" << endl;
    mpo = make_shared<SimplifiedMPO<SU2>>(mpo, make_shared<RuleQC<SU2>>(), true);
    cout << "MPO simplification end .. T = " << t.get_time() << endl;
    // cout << mpo->get_blocking_formulas() << endl;
    // abort();

    uint16_t bond_dim = 250;

    // MPSInfo
    shared_ptr<MPSInfo<SU2>> mps_info = make_shared<MPSInfo<SU2>>(
        norb, vaccum, target, hamil.basis, hamil.orb_sym, hamil.n_syms);
    if (occs.size() == 0)
        mps_info->set_bond_dimension(bond_dim);
    else {
        assert(occs.size() == norb);
        // for (size_t i = 0; i < occs.size(); i++)
        //     cout << occs[i] << " ";
        mps_info->set_bond_dimension_using_occ(bond_dim, occs);
    }
    // cout << "left min dims = ";
    // for (int i = 0; i <= norb; i++)
    //     cout << mps_info->left_dims_fci[i].n << " ";
    // cout << endl;
    // cout << "right min dims = ";
    // for (int i = 0; i <= norb; i++)
    //     cout << mps_info->right_dims_fci[i].n << " ";
    // cout << endl;
    // cout << "left q dims = ";
    // for (int i = 0; i <= norb; i++)
    //     cout << mps_info->left_dims[i].n << " ";
    // cout << endl;
    // cout << "right q dims = ";
    // for (int i = 0; i <= norb; i++)
    //     cout << mps_info->right_dims[i].n << " ";
    // cout << endl;
    cout << "left dims = ";
    for (int i = 0; i <= norb; i++)
        cout << mps_info->left_dims[i].n_states_total << " ";
    cout << endl;
    cout << "right dims = ";
    for (int i = 0; i <= norb; i++)
        cout << mps_info->right_dims[i].n_states_total << " ";
    cout << endl;
    // abort();

    // MPS
    // Random::rand_seed(0);
    // int x = Random::rand_int(0, 1000000);
    Random::rand_seed(384666);
    // cout << "Random = " << x << endl;
    shared_ptr<MPS<SU2>> mps = make_shared<MPS<SU2>>(norb, 0, 2);
    mps->initialize(mps_info);
    mps->random_canonicalize();

    // for (int i = 0; i < mps->n_sites; i++)
    //     if (mps->tensors[i] != nullptr)
    //         cout << *mps->tensors[i] << endl;
    // abort();

    // MPS/MPSInfo save mutable
        mps->save_mutable();
    mps->deallocate();
    mps_info->save_mutable();
    mps_info->deallocate_mutable();

    frame->activate(0);
    cout << "persistent memory used :: I = " << ialloc->used
         << " D = " << dalloc->used << endl;
    frame->activate(1);
    cout << "exclusive  memory used :: I = " << ialloc->used
         << " D = " << dalloc->used << endl;
    // ME
    hamil.opf->seq->mode = SeqTypes::Simple;
    shared_ptr<MovingEnvironment<SU2>> me =
        make_shared<MovingEnvironment<SU2>>(mpo, mps, mps, "DMRG");
    t.get_time();
    cout << "INIT start" << endl;
    me->init_environments(false);
    cout << "INIT end .. T = " << t.get_time() << endl;

    cout << *frame << endl;
    frame->activate(0);

    // DMRG
    // vector<uint16_t> bdims = {250, 250, 250, 250, 250, 500, 500, 500,
    //                           500, 500, 750, 750, 750, 750, 750};
    // vector<double> noises = {1E-6, 1E-6, 1E-6, 1E-6, 1E-6, 1E-7, 1E-7, 1E-7, 1E-7, 1E-7, 1E-8, 1E-8, 1E-8, 1E-8, 1E-8, 0.0};
    vector<uint16_t> bdims = {bond_dim};
    vector<double> noises = {1E-6};
    shared_ptr<DMRG<SU2>> dmrg = make_shared<DMRG<SU2>>(me, bdims, noises);
    dmrg->solve(30, true);

    // deallocate persistent stack memory
    mps_info->deallocate();
    mpo->deallocate();
    hamil.deallocate();
    fcidump->deallocate();
}