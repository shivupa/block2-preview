
#include "block2_core.hpp"
#include "block2_dmrg.hpp"
#include <gtest/gtest.h>

using namespace block2;

template <typename FL> class TestTTODMRGN2STO3G : public ::testing::Test {
  protected:
    size_t isize = 1L << 24;
    size_t dsize = 1L << 32;
    typedef typename GMatrix<FL>::FP FP;

    template <typename S>
    void test_dmrg(const vector<vector<S>> &targets,
                   const vector<vector<FL>> &energies,
                   const shared_ptr<HamiltonianQC<S, FL>> &hamil,
                   const string &name, DecompositionTypes dt, NoiseTypes nt,
                   int tto);
    void SetUp() override {
        cout << "BOND INTEGER SIZE = " << sizeof(ubond_t) << endl;
        Random::rand_seed(0);
        frame_() = make_shared<DataFrame>(isize, dsize, "nodex");
        frame_()->use_main_stack = false;
        frame_()->minimal_disk_usage = true;
        threading_() = make_shared<Threading>(
            ThreadingTypes::OperatorBatchedGEMM | ThreadingTypes::Global, 8, 8,
            1);
        threading_()->seq_type = SeqTypes::Tasked;
        cout << *threading_() << endl;
    }
    void TearDown() override {
        frame_()->activate(0);
        assert(ialloc_()->used == 0 && dalloc_()->used == 0);
        frame_() = nullptr;
    }
};

template <typename FL>
template <typename S>
void TestTTODMRGN2STO3G<FL>::test_dmrg(
    const vector<vector<S>> &targets, const vector<vector<FL>> &energies,
    const shared_ptr<HamiltonianQC<S, FL>> &hamil, const string &name,
    DecompositionTypes dt, NoiseTypes nt, int tto) {
    Timer t;
    t.get_time();
    // MPO construction
    cout << "MPO start" << endl;
    shared_ptr<MPO<S, FL>> mpo =
        make_shared<MPOQC<S, FL>>(hamil, QCTypes::Conventional);
    cout << "MPO end .. T = " << t.get_time() << endl;

    // MPO simplification
    cout << "MPO simplification start" << endl;
    mpo = make_shared<SimplifiedMPO<S, FL>>(
        mpo, make_shared<RuleQC<S, FL>>(), true, true,
        OpNamesSet({OpNames::R, OpNames::RD}));
    cout << "MPO simplification end .. T = " << t.get_time() << endl;

    ubond_t bond_dim = 200;
    vector<ubond_t> bdims = {bond_dim};
    vector<FP> noises = {1E-8, 1E-9, 0.0};

    t.get_time();

    Random::rand_seed(0);

    for (int i = 0; i < (int)targets.size(); i++)
        for (int j = 0, k = 0; j < (int)targets[i].size(); j++) {

            S target = targets[i][j];

            shared_ptr<MPSInfo<S>> mps_info = make_shared<MPSInfo<S>>(
                hamil->n_sites, hamil->vacuum, target, hamil->basis);
            mps_info->set_bond_dimension(bond_dim);

            // MPS
            shared_ptr<MPS<S, FL>> mps =
                make_shared<MPS<S, FL>>(hamil->n_sites, 0, 2);
            mps->initialize(mps_info);
            mps->random_canonicalize();

            // MPS/MPSInfo save mutable
            mps->save_mutable();
            mps->deallocate();
            mps_info->save_mutable();
            mps_info->deallocate_mutable();

            // ME
            shared_ptr<MovingEnvironment<S, FL, FL>> me =
                make_shared<MovingEnvironment<S, FL, FL>>(mpo, mps, mps,
                                                          "DMRG");
            me->init_environments(false);
            me->delayed_contraction = OpNamesSet::normal_ops();
            me->cached_contraction = true;

            // DMRG
            shared_ptr<DMRG<S, FL, FL>> dmrg =
                make_shared<DMRG<S, FL, FL>>(me, bdims, noises);
            dmrg->iprint = 0;
            dmrg->decomp_type = dt;
            dmrg->noise_type = nt;
            dmrg->davidson_soft_max_iter = 4000;
            dmrg->solve(tto, mps->center == 0, 0);

            me->dot = 1;
            FL energy = dmrg->solve(10, mps->center == 0, 1E-8);

            // deallocate persistent stack memory
            mps_info->deallocate();

            cout << "== " << name << "|" << tto << " ==" << setw(20) << target
                 << " E = " << fixed << setw(22) << setprecision(12) << energy
                 << " error = " << scientific << setprecision(3) << setw(10)
                 << (energy - energies[i][j]) << " T = " << fixed << setw(10)
                 << setprecision(3) << t.get_time() << endl;

            if (abs(energy - energies[i][j]) >= 1E-7 && k < 3) {
                k++, j--;
                cout << "!!! RETRY ... " << endl;
                continue;
            }

            EXPECT_LT(abs(energy - energies[i][j]), 1E-7);

            k = 0;
        }

    mpo->deallocate();
}

#ifdef _USE_COMPLEX
typedef ::testing::Types<complex<double>, double> TestFL;
#else
typedef ::testing::Types<double> TestFL;
#endif

TYPED_TEST_CASE(TestTTODMRGN2STO3G, TestFL);

TYPED_TEST(TestTTODMRGN2STO3G, TestSU2) {
    using FL = TypeParam;

    shared_ptr<FCIDUMP<FL>> fcidump = make_shared<FCIDUMP<FL>>();
    PGTypes pg = PGTypes::D2H;
    string filename = "data/N2.STO3G.FCIDUMP";
    fcidump->read(filename);
    vector<uint8_t> orbsym = fcidump->template orb_sym<uint8_t>();
    transform(orbsym.begin(), orbsym.end(), orbsym.begin(),
              PointGroup::swap_pg(pg));

    SU2 vacuum(0);

    vector<vector<SU2>> targets(8);
    for (int i = 0; i < 8; i++) {
        targets[i].resize(3);
        for (int j = 0; j < 3; j++)
            targets[i][j] = SU2(fcidump->n_elec(), j * 2, i);
    }

    vector<vector<FL>> energies(8);
    energies[0] = {-107.654122447525, -106.939132859668, -107.031449471627};
    energies[1] = {-106.959626154680, -106.999600016661, -106.633790589321};
    energies[2] = {-107.306744734756, -107.356943001688, -106.931515926732};
    energies[3] = {-107.306744734756, -107.356943001688, -106.931515926731};
    energies[4] = {-107.223155479270, -107.279409754727, -107.012640794842};
    energies[5] = {-107.208347039017, -107.343458537272, -106.227634428741};
    energies[6] = {-107.116397543375, -107.208021870379, -107.070427868786};
    energies[7] = {-107.116397543375, -107.208021870379, -107.070427868786};

    int norb = fcidump->n_sites();
    shared_ptr<HamiltonianQC<SU2, FL>> hamil =
        make_shared<HamiltonianQC<SU2, FL>>(vacuum, norb, orbsym, fcidump);

    targets.resize(2);
    energies.resize(2);

    for (int tto = 4; tto < 8; tto++) {
        this->template test_dmrg<SU2>(targets, energies, hamil, "SU2",
                                      DecompositionTypes::DensityMatrix,
                                      NoiseTypes::DensityMatrix, tto);
        this->template test_dmrg<SU2>(targets, energies, hamil, "SU2 RED PERT",
                                      DecompositionTypes::DensityMatrix,
                                      NoiseTypes::ReducedPerturbative, tto);
        this->template test_dmrg<SU2>(
            targets, energies, hamil, "SU2 SVD RED PERT",
            DecompositionTypes::SVD, NoiseTypes::ReducedPerturbative, tto);
    }

    hamil->deallocate();
    fcidump->deallocate();
}

TYPED_TEST(TestTTODMRGN2STO3G, TestSZ) {
    using FL = TypeParam;

    shared_ptr<FCIDUMP<FL>> fcidump = make_shared<FCIDUMP<FL>>();
    PGTypes pg = PGTypes::D2H;
    string filename = "data/N2.STO3G.FCIDUMP";
    fcidump->read(filename);
    vector<uint8_t> orbsym = fcidump->template orb_sym<uint8_t>();
    transform(orbsym.begin(), orbsym.end(), orbsym.begin(),
              PointGroup::swap_pg(pg));

    SZ vacuum(0);

    vector<vector<SZ>> targets(8);
    for (int i = 0; i < 8; i++) {
        targets[i].resize(5);
        for (int j = 0; j < 5; j++)
            targets[i][j] = SZ(fcidump->n_elec(), (j - 2) * 2, i);
    }

    vector<vector<FL>> energies(8);
    energies[0] = {-107.031449471627, -107.031449471627, -107.654122447525,
                   -107.031449471627, -107.031449471627};
    energies[1] = {-106.633790589321, -106.999600016661, -106.999600016661,
                   -106.999600016661, -106.633790589321};
    energies[2] = {-106.931515926732, -107.356943001688, -107.356943001688,
                   -107.356943001688, -106.931515926732};
    energies[3] = {-106.931515926731, -107.356943001688, -107.356943001688,
                   -107.356943001688, -106.931515926731};
    energies[4] = {-107.012640794842, -107.279409754727, -107.279409754727,
                   -107.279409754727, -107.012640794842};
    energies[5] = {-106.227634428741, -107.343458537272, -107.343458537272,
                   -107.343458537272, -106.227634428741};
    energies[6] = {-107.070427868786, -107.208021870379, -107.208021870379,
                   -107.208021870379, -107.070427868786};
    energies[7] = {-107.070427868786, -107.208021870379, -107.208021870379,
                   -107.208021870379, -107.070427868786};

    int norb = fcidump->n_sites();
    shared_ptr<HamiltonianQC<SZ, FL>> hamil =
        make_shared<HamiltonianQC<SZ, FL>>(vacuum, norb, orbsym, fcidump);

    targets.resize(2);
    energies.resize(2);

    for (int tto = 4; tto < 8; tto++) {
        this->template test_dmrg<SZ>(targets, energies, hamil, "SZ",
                                     DecompositionTypes::DensityMatrix,
                                     NoiseTypes::DensityMatrix, tto);
        this->template test_dmrg<SZ>(targets, energies, hamil, "SZ RED PERT",
                                     DecompositionTypes::DensityMatrix,
                                     NoiseTypes::ReducedPerturbative, tto);
        this->template test_dmrg<SZ>(targets, energies, hamil,
                                     "SZ SVD RED PERT", DecompositionTypes::SVD,
                                     NoiseTypes::ReducedPerturbative, tto);
    }

    hamil->deallocate();
    fcidump->deallocate();
}