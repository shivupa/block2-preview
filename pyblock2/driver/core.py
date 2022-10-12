#  block2: Efficient MPO implementation of quantum chemistry DMRG
#  Copyright (C) 2022 Huanchen Zhai <hczhai@caltech.edu>
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program. If not, see <https://www.gnu.org/licenses/>.
#
#

from enum import IntFlag


class SymmetryTypes(IntFlag):
    Nothing = 0
    SU2 = 1
    SZ = 2
    SGF = 4
    SGB = 8
    CPX = 16
    SP = 32
    SGFCPX = 16 | 4
    SPCPX = 32 | 16


class ParallelTypes(IntFlag):
    Nothing = 0
    I = 1
    J = 2
    SI = 3
    SJ = 4
    SIJ = 5


class MPOAlgorithmTypes(IntFlag):
    Nothing = 0
    Bipartite = 1
    SVD = 2
    Rescaled = 4
    Fast = 8
    Blocked = 16
    Sum = 32
    Constrained = 64
    Disjoint = 128
    DisjointSVD = 128 | 2
    BlockedSumDisjointSVD = 128 | 32 | 16 | 2
    FastBlockedSumDisjointSVD = 128 | 32 | 16 | 8 | 2
    BlockedRescaledSumDisjointSVD = 128 | 32 | 16 | 4 | 2
    FastBlockedRescaledSumDisjointSVD = 128 | 32 | 16 | 8 | 4 | 2
    BlockedDisjointSVD = 128 | 16 | 2
    FastBlockedDisjointSVD = 128 | 16 | 8 | 2
    BlockedRescaledDisjointSVD = 128 | 16 | 4 | 2
    FastBlockedRescaledDisjointSVD = 128 | 16 | 8 | 4 | 2
    RescaledDisjointSVD = 128 | 4 | 2
    FastDisjointSVD = 128 | 8 | 2
    FastRescaledDisjointSVD = 128 | 8 | 4 | 2
    ConstrainedSVD = 64 | 2
    BlockedSumConstrainedSVD = 64 | 32 | 16 | 2
    FastBlockedSumConstrainedSVD = 64 | 32 | 16 | 8 | 2
    BlockedRescaledSumConstrainedSVD = 64 | 32 | 16 | 4 | 2
    FastBlockedRescaledSumConstrainedSVD = 64 | 32 | 16 | 8 | 4 | 2
    BlockedConstrainedSVD = 64 | 16 | 2
    FastBlockedConstrainedSVD = 64 | 16 | 8 | 2
    BlockedRescaledConstrainedSVD = 64 | 16 | 4 | 2
    FastBlockedRescaledConstrainedSVD = 64 | 16 | 8 | 4 | 2
    RescaledConstrainedSVD = 64 | 4 | 2
    FastConstrainedSVD = 64 | 8 | 2
    FastRescaledConstrainedSVD = 64 | 8 | 4 | 2
    BlockedSumSVD = 32 | 16 | 2
    FastBlockedSumSVD = 32 | 16 | 8 | 2
    BlockedRescaledSumSVD = 32 | 16 | 4 | 2
    FastBlockedRescaledSumSVD = 32 | 16 | 8 | 4 | 2
    BlockedSumBipartite = 32 | 16 | 1
    FastBlockedSumBipartite = 32 | 16 | 8 | 1
    BlockedSVD = 16 | 2
    FastBlockedSVD = 16 | 8 | 2
    BlockedRescaledSVD = 16 | 4 | 2
    FastBlockedRescaledSVD = 16 | 8 | 4 | 2
    BlockedBipartite = 16 | 1
    FastBlockedBipartite = 16 | 8 | 1
    RescaledSVD = 4 | 2
    FastSVD = 8 | 2
    FastRescaledSVD = 8 | 4 | 2
    FastBipartite = 8 | 1
    NC = 256
    CN = 512
    Conventional = 1024
    ConventionalNC = 1024 | 256
    ConventionalCN = 1024 | 512
    NoTranspose = 2048
    NoRIntermed = 4096
    NoTransConventional = 2048 | 1024
    NoTransConventionalNC = 2048 | 1024 | 256
    NoTransConventionalCN = 2048 | 1024 | 512
    NoRIntermedConventional = 4096 | 1024
    NoTransNoRIntermedConventional = 4096 | 2048 | 1024


class Block2Wrapper:
    def __init__(self, symm_type=SymmetryTypes.SU2):
        import block2 as b

        self.b = b
        self.symm_type = symm_type
        has_cpx = hasattr(b, "cpx")
        has_sp = hasattr(b, "sp")
        has_spcpx = has_sp and hasattr(b.sp, "cpx")
        if SymmetryTypes.SPCPX in symm_type:
            self.VectorFL = b.VectorComplexFloat
            self.VectorFP = b.VectorFloat
            self.bx = b.sp.cpx
            self.bc = self.bx
        elif SymmetryTypes.CPX in symm_type:
            self.VectorFL = b.VectorComplexDouble
            self.VectorFP = b.VectorDouble
            self.bx = b.cpx
            self.bc = self.bx
        elif SymmetryTypes.SP in symm_type:
            self.VectorFL = b.VectorFloat
            self.VectorFP = b.VectorFloat
            self.bx = b.sp
            self.bc = b.sp.cpx if has_spcpx else None
        else:
            self.VectorFL = b.VectorDouble
            self.VectorFP = b.VectorDouble
            self.bx = b
            self.bc = b.cpx if has_cpx else None
        if SymmetryTypes.SU2 in symm_type:
            self.bs = self.bx.su2
            self.bcs = self.bc.su2 if self.bc is not None else None
            self.brs = b.su2
            self.SX = b.SU2
            self.VectorSX = b.VectorSU2
        elif SymmetryTypes.SZ in symm_type:
            self.bs = self.bx.sz
            self.bcs = self.bc.sz if self.bc is not None else None
            self.brs = b.sz
            self.SX = b.SZ
            self.VectorSX = b.VectorSZ
        elif SymmetryTypes.SGF in symm_type:
            self.bs = self.bx.sgf
            self.bcs = self.bc.sgf if self.bc is not None else None
            self.brs = b.sgf
            self.SX = b.SGF
            self.VectorSX = b.VectorSGF
        elif SymmetryTypes.SGB in symm_type:
            self.bs = self.bx.sgb
            self.bcs = self.bc.sgb if self.bc is not None else None
            self.brs = b.sgb
            self.SX = b.SGB
            self.VectorSX = b.VectorSGB


class DMRGDriver:
    def __init__(
        self,
        stack_mem=1 << 30,
        scratch="./nodex",
        restart_dir=None,
        n_threads=None,
        symm_type=SymmetryTypes.SU2,
        mpi=None,
    ):
        if mpi is not None and mpi:
            self.mpi = True
        else:
            self.mpi = None
            self.prule = None

        self._scratch = scratch
        self._restart_dir = restart_dir
        self.stack_mem = stack_mem
        self.symm_type = symm_type
        bw = self.bw

        if n_threads is None:
            n_threads = bw.b.Global.threading.n_threads_global
        bw.b.Global.threading = bw.b.Threading(
            bw.b.ThreadingTypes.OperatorBatchedGEMM | bw.b.ThreadingTypes.Global,
            n_threads,
            n_threads,
            1,
        )
        bw.b.Global.threading.seq_type = bw.b.SeqTypes.Tasked
        self.reorder_idx = None
        self.pg = "c1"
        self.orb_sym = None
        self.ghamil = None

    @property
    def symm_type(self):
        return self._symm_type

    @symm_type.setter
    def symm_type(self, symm_type):
        self._symm_type = symm_type
        self.set_symm_type(symm_type)

    @property
    def scratch(self):
        return self._scratch

    @scratch.setter
    def scratch(self, scratch):
        self._scratch = scratch
        self.frame.save_dir = scratch
        self.frame.mps_dir = scratch
        self.frame.mpo_dir = scratch

    @property
    def restart_dir(self):
        return self._restart_dir

    @restart_dir.setter
    def restart_dir(self, restart_dir):
        self._restart_dir = restart_dir
        self.frame.restart_dir = restart_dir

    def parallelize_integrals(self, para_type, h1e, g2e, const):
        import numpy as np

        if para_type == ParallelTypes.Nothing or self.mpi is None:
            return h1e, g2e, const
        if h1e is not None:
            ixs = np.mgrid[tuple(slice(x) for x in h1e.shape)].reshape((h1e.ndim, -1)).T
            sixs = np.sort(ixs, axis=1)
        if g2e is not None:
            gixs = (
                np.mgrid[tuple(slice(x) for x in g2e.shape)].reshape((g2e.ndim, -1)).T
            )
            gsixs = np.sort(gixs, axis=1)

        if para_type == ParallelTypes.I:
            if h1e is not None:
                mask1 = ixs[:, 0] % self.mpi.size == self.mpi.rank
            if g2e is not None:
                mask2 = gixs[:, 0] % self.mpi.size == self.mpi.rank
        elif para_type == ParallelTypes.SI:
            if h1e is not None:
                mask1 = sixs[:, 0] % self.mpi.size == self.mpi.rank
            if g2e is not None:
                mask2 = gsixs[:, 0] % self.mpi.size == self.mpi.rank
        elif para_type == ParallelTypes.J:
            if h1e is not None:
                mask1 = ixs[:, 1] % self.mpi.size == self.mpi.rank
            if g2e is not None:
                mask2 = gixs[:, 1] % self.mpi.size == self.mpi.rank
        elif para_type == ParallelTypes.SJ:
            if h1e is not None:
                mask1 = sixs[:, 1] % self.mpi.size == self.mpi.rank
            if g2e is not None:
                mask2 = gsixs[:, 1] % self.mpi.size == self.mpi.rank
        elif para_type == ParallelTypes.SIJ:
            if h1e is not None:
                mask1 = sixs[:, 0] % self.mpi.size == self.mpi.rank
            if g2e is not None:
                mask2a = (gsixs[:, 1] == gsixs[:, 2]) & (
                    gsixs[:, 1] % self.mpi.size == self.mpi.rank
                )
                mask2b = (
                    (gsixs[:, 1] != gsixs[:, 2])
                    & (gsixs[:, 0] <= gsixs[:, 1])
                    & (
                        (gsixs[:, 1] * (gsixs[:, 1] + 1) // 2 + gsixs[:, 0])
                        % self.mpi.size
                        == self.mpi.rank
                    )
                )
                mask2c = (
                    (gsixs[:, 1] != gsixs[:, 2])
                    & (gsixs[:, 0] > gsixs[:, 1])
                    & (
                        (gsixs[:, 0] * (gsixs[:, 0] + 1) // 2 + gsixs[:, 1])
                        % self.mpi.size
                        == self.mpi.rank
                    )
                )
                mask2 = mask2a | mask2b | mask2c
        if h1e is not None:
            h1e[~mask1.reshape(h1e.shape)] = 0.0
        if g2e is not None:
            g2e[~mask2.reshape(g2e.shape)] = 0.0
        if self.mpi.rank != self.mpi.root:
            const = 0
        return h1e, g2e, const

    def set_symm_type(self, symm_type, reset_frame=True):
        self.bw = Block2Wrapper(symm_type)
        bw = self.bw

        # reset_frame only required when switching between dp/sp
        if reset_frame:
            if SymmetryTypes.SP not in bw.symm_type:
                bw.b.Global.frame = bw.b.DoubleDataFrame(
                    int(self.stack_mem * 0.1), int(self.stack_mem * 0.9), self.scratch
                )
                bw.b.Global.frame.fp_codec = bw.b.DoubleFPCodec(1e-16, 1024)
                bw.b.Global.frame_float = None
                self.frame = bw.b.Global.frame
            else:
                bw.b.Global.frame_float = bw.b.FloatDataFrame(
                    int(self.stack_mem * 0.1), int(self.stack_mem * 0.9), self.scratch
                )
                bw.b.Global.frame_float.fp_codec = bw.b.FloatFPCodec(1e-16, 1024)
                bw.b.Global.frame = None
                self.frame = bw.b.Global.frame_float
        self.frame.minimal_disk_usage = True
        self.frame.use_main_stack = False

        if self.mpi:
            self.mpi = bw.brs.MPICommunicator()
            self.prule = bw.bs.ParallelRuleSimple(
                bw.b.ParallelSimpleTypes.Nothing, self.mpi
            )

        if self.restart_dir is not None:
            import os

            if self.mpi is None or self.mpi.rank == self.mpi.root:
                if not os.path.isdir(self.restart_dir):
                    os.makedirs(self.restart_dir)
            if self.mpi is not None:
                self.mpi.barrier()
            self.frame.restart_dir = self.restart_dir

    def initialize_system(
        self,
        n_sites,
        n_elec=0,
        spin=0,
        pg_irrep=None,
        orb_sym=None,
        heis_twos=-1,
        heis_twosz=0,
        singlet_embedding=True,
    ):
        bw = self.bw
        self.vacuum = bw.SX(0, 0, 0)
        if heis_twos != -1 and bw.SX == bw.b.SU2 and n_elec == 0:
            n_elec = n_sites * heis_twos
        if pg_irrep is None:
            if hasattr(self, "pg_irrep"):
                pg_irrep = self.pg_irrep
            else:
                pg_irrep = 0
        if not SymmetryTypes.SU2 in bw.symm_type or heis_twos != -1:
            singlet_embedding = False
        if singlet_embedding:
            assert heis_twosz == 0
            self.target = bw.SX(n_elec + spin % 2, 0, pg_irrep)
            self.left_vacuum = bw.SX(spin % 2, spin, 0)
        else:
            self.target = bw.SX(
                n_elec if heis_twosz == 0 else heis_twosz, spin, pg_irrep
            )
            self.left_vacuum = self.vacuum
        self.n_sites = n_sites
        self.heis_twos = heis_twos
        if orb_sym is None:
            self.orb_sym = bw.b.VectorUInt8([0] * self.n_sites)
        else:
            self.orb_sym = bw.b.VectorUInt8(orb_sym)
        self.ghamil = bw.bs.GeneralHamiltonian(
            self.vacuum, self.n_sites, self.orb_sym, self.heis_twos
        )

    def write_fcidump(self, h1e, g2e, ecore=0, filename=None, pg="d2h"):
        bw = self.bw
        import numpy as np

        fcidump = bw.bx.FCIDUMP()
        swap_pg = getattr(bw.b.PointGroup, "swap_" + pg)
        fw_map = np.array([swap_pg(x) for x in range(1, 9)])
        bk_map = np.argsort(fw_map) + 1
        if SymmetryTypes.SZ in bw.symm_type:
            mh1e = tuple(x.flatten() for x in h1e)
            mg2e = tuple(x.flatten() for x in g2e)
            fcidump.initialize_sz(
                self.n_sites,
                self.target.n,
                self.target.twos,
                bk_map[self.target.pg],
                ecore,
                mh1e,
                mg2e,
            )
        elif g2e is not None:
            fcidump.initialize_su2(
                self.n_sites,
                self.target.n,
                self.target.twos,
                bk_map[self.target.pg],
                ecore,
                h1e.flatten(),
                g2e.flatten(),
            )
        else:
            fcidump.initialize_h1e(
                self.n_sites,
                self.target.n,
                self.target.twos,
                bk_map[self.target.pg],
                ecore,
                h1e.flatten(),
            )
        fcidump.orb_sym = bw.b.VectorUInt8([bk_map[x] for x in self.orb_sym])
        if filename is not None:
            fcidump.write(filename)
        return fcidump

    def read_fcidump(self, filename, pg="d2h", rescale=None, iprint=1):
        bw = self.bw
        fcidump = bw.bx.FCIDUMP()
        fcidump.read(filename)
        self.pg = pg
        swap_pg = getattr(bw.b.PointGroup, "swap_" + pg)
        self.orb_sym = bw.b.VectorUInt8(map(swap_pg, fcidump.orb_sym))
        for x in self.orb_sym:
            if x == 8:
                raise RuntimeError("Wrong point group symmetry : ", pg)
        self.n_sites = fcidump.n_sites
        self.n_elec = fcidump.n_elec
        self.spin = fcidump.twos
        self.pg_irrep = swap_pg(fcidump.isym)
        if rescale is not None:
            if iprint >= 1:
                print("original const = ", fcidump.const_e)
            if isinstance(rescale, float):
                fcidump.rescale(rescale)
            elif rescale:
                fcidump.rescale()
            if iprint >= 1:
                print("rescaled const = ", fcidump.const_e)
        self.const_e = fcidump.const_e
        import numpy as np

        if iprint >= 1:
            print("symmetrize error = ", fcidump.symmetrize(self.orb_sym))

        self.h1e = np.array(fcidump.h1e_matrix(), copy=False).reshape(
            (self.n_sites,) * 2
        )
        self.g2e = np.array(fcidump.g2e_1fold(), copy=False).reshape(
            (self.n_sites,) * 4
        )
        return fcidump

    def su2_to_sgf(self):
        bw = self.bw
        import numpy as np

        gh1e = np.zeros((self.n_sites * 2,) * 2)
        gg2e = np.zeros((self.n_sites * 2,) * 4)

        for i in range(self.n_sites * 2):
            for j in range(i % 2, self.n_sites * 2, 2):
                gh1e[i, j] = self.h1e[i // 2, j // 2]

        for i in range(self.n_sites * 2):
            for j in range(i % 2, self.n_sites * 2, 2):
                for k in range(self.n_sites * 2):
                    for l in range(k % 2, self.n_sites * 2, 2):
                        gg2e[i, j, k, l] = self.g2e[i // 2, j // 2, k // 2, l // 2]

        self.h1e = gh1e
        self.g2e = gg2e
        self.n_sites = self.n_sites * 2
        if hasattr(self, "orb_sym"):
            self.orb_sym = bw.b.VectorUInt8(
                [self.orb_sym[i // 2] for i in range(self.n_sites)]
            )

    def get_conventional_qc_mpo(self, fcidump, algo_type=None, iprint=1):
        """This method cannot take care of parallelization!"""
        bw = self.bw
        hamil = bw.bs.HamiltonianQC(self.vacuum, self.n_sites, self.orb_sym, fcidump)
        import time
        tt = time.perf_counter()
        if algo_type is not None and MPOAlgorithmTypes.NC in algo_type:
            mpo = bw.bs.MPOQC(hamil, bw.b.QCTypes.NC, "HQC")
        elif algo_type is not None and MPOAlgorithmTypes.CN in algo_type:
            mpo = bw.bs.MPOQC(hamil, bw.b.QCTypes.CN, "HQC")
        elif algo_type is None or MPOAlgorithmTypes.Conventional in algo_type:
            mpo = bw.bs.MPOQC(hamil, bw.b.QCTypes.Conventional, "HQC")
        else:
            raise RuntimeError("Invalid conventional mpo algo type:", algo_type)
        if algo_type is not None and MPOAlgorithmTypes.NoTranspose in algo_type:
            rule = bw.bs.NoTransposeRule(bw.bs.RuleQC())
        else:
            rule = bw.bs.RuleQC()
        use_r_intermediates = (
            algo_type is None or MPOAlgorithmTypes.NoRIntermed not in algo_type
        )

        if iprint:
            nnz, sz, bdim = mpo.get_summary()
            print('Ttotal = %10.3f MPO method = %s bond dimension = %7d NNZ = %12d SIZE = %12d SPT = %6.4f'
                % (time.perf_counter() - tt, algo_type.name, bdim, nnz, sz, (1.0 * sz - nnz) / sz))

        mpo = bw.bs.SimplifiedMPO(
            mpo,
            rule,
            True,
            use_r_intermediates,
            bw.b.OpNamesSet((bw.b.OpNames.R, bw.b.OpNames.RD)),
        )
        if self.mpi:
            mpo = bw.bs.ParallelMPO(mpo, self.prule)
        return mpo

    def get_identity_mpo(self):
        return self.get_mpo(self.expr_builder().add_term("", [], 1.0).finalize())

    def get_qc_mpo(
        self,
        h1e,
        g2e,
        ecore=0,
        para_type=None,
        reorder=None,
        cutoff=1e-20,
        integral_cutoff=1e-20,
        post_integral_cutoff=1e-20,
        algo_type=None,
        normal_order_ref=None,
        symmetrize=True,
        sum_mpo_mod=-1,
        compute_accurate_svd_error=True,
        csvd_sparsity=0.0,
        csvd_eps=1e-10,
        csvd_max_iter=1000,
        disjoint_levels=None,
        disjoint_all_blocks=False,
        disjoint_multiplier=1.0,
        iprint=1,
    ):
        import numpy as np

        bw = self.bw

        if SymmetryTypes.SZ in bw.symm_type:
            if h1e is not None and isinstance(h1e, np.ndarray) and h1e.ndim == 2:
                h1e = (h1e, h1e)
            if g2e is not None and isinstance(g2e, np.ndarray) and g2e.ndim == 4:
                g2e = (g2e, g2e, g2e)
        elif SymmetryTypes.SGF in bw.symm_type:
            if (
                h1e is not None
                and hasattr(self, "n_sites")
                and len(h1e) * 2 == self.n_sites
            ):
                gh1e = np.zeros((len(h1e) * 2,) * 2)
                gh1e[0::2, 0::2] = gh1e[1::2, 1::2] = h1e
                h1e = gh1e
            if (
                g2e is not None
                and hasattr(self, "n_sites")
                and len(g2e) * 2 == self.n_sites
            ):
                gg2e = np.zeros((len(g2e) * 2,) * 4)
                gg2e[0::2, 0::2, 0::2, 0::2] = gg2e[0::2, 0::2, 1::2, 1::2] = g2e
                gg2e[1::2, 1::2, 0::2, 0::2] = gg2e[1::2, 1::2, 1::2, 1::2] = g2e
                g2e = gg2e

        if symmetrize and self.orb_sym is not None:
            x = np.array(self.orb_sym, dtype=int)
            error = 0
            if SymmetryTypes.SZ in bw.symm_type:
                if h1e is not None:
                    mask = (x[:, None] ^ x[None, :]) != 0
                    error += sum(np.sum(np.abs(h[mask])) for h in h1e)
                    h1e[0][mask] = 0
                    h1e[1][mask] = 0
                if g2e is not None:
                    mask = (
                        x[:, None, None, None]
                        ^ x[None, :, None, None]
                        ^ x[None, None, :, None]
                        ^ x[None, None, None, :]
                    ) != 0
                    error += sum(np.sum(np.abs(g[mask])) for g in g2e) * 0.5
                    error += np.sum(np.abs(g2e[1][mask])) * 0.5
                    g2e[0][mask] = 0
                    g2e[1][mask] = 0
                    g2e[2][mask] = 0
            else:
                if h1e is not None:
                    mask = (x[:, None] ^ x[None, :]) != 0
                    error += np.sum(np.abs(h1e[mask]))
                    h1e[mask] = 0
                if g2e is not None:
                    mask = (
                        x[:, None, None, None]
                        ^ x[None, :, None, None]
                        ^ x[None, None, :, None]
                        ^ x[None, None, None, :]
                    ) != 0
                    error += np.sum(np.abs(g2e[mask])) * 0.5
                    g2e[mask] = 0
            if iprint:
                print("integral symmetrize error = ", error)

        if integral_cutoff != 0:
            error = 0
            if SymmetryTypes.SZ in bw.symm_type:
                if h1e is not None:
                    for i in range(2):
                        mask = np.abs(h1e[i]) < integral_cutoff
                        error += np.sum(np.abs(h1e[i][mask]))
                        h1e[i][mask] = 0
                if g2e is not None:
                    for i in range(3):
                        mask = np.abs(g2e[i]) < integral_cutoff
                        error += np.sum(np.abs(g2e[i][mask])) * (1 if i == 1 else 0.5)
                        g2e[i][mask] = 0
            else:
                if h1e is not None:
                    mask = np.abs(h1e) < integral_cutoff
                    error += np.sum(np.abs(h1e[mask]))
                    h1e[mask] = 0
                if g2e is not None:
                    mask = np.abs(g2e) < integral_cutoff
                    error += np.sum(np.abs(g2e[mask])) * 0.5
                    g2e[mask] = 0
            if iprint:
                print("integral cutoff error = ", error)

        if reorder is not None:
            if isinstance(reorder, np.ndarray):
                idx = reorder
            elif reorder == "irrep":
                assert self.orb_sym is not None
                if self.pg == "d2h":
                    # D2H
                    # 0   1   2   3   4   5   6   7   (XOR)
                    # A1g B1g B2g B3g A1u B1u B2u B3u
                    # optimal
                    # A1g B1u B3u B2g B2u B3g B1g A1u
                    optimal_reorder = [0, 6, 3, 5, 7, 1, 4, 2, 8]
                elif self.pg == "c2v":
                    # C2V
                    # 0  1  2  3  (XOR)
                    # A1 A2 B1 B2
                    # optimal
                    # A1 B1 B2 A2
                    optimal_reorder = [0, 3, 1, 2, 4]
                else:
                    optimal_reorder = [0, 6, 3, 5, 7, 1, 4, 2, 8]
                orb_opt = [optimal_reorder[x] for x in np.array(self.orb_sym)]
                idx = np.argsort(orb_opt)
            elif reorder == "fiedler" or reorder == True:
                idx = self.orbital_reordering(h1e, g2e)
            else:
                raise RuntimeError("Unknown reorder", reorder)
            if iprint:
                print("reordering = ", idx)
            self.reorder_idx = idx
            if SymmetryTypes.SZ in bw.symm_type:
                for i in enumerate(len(h1e)):
                    h1e[i] = h1e[i][idx][:, idx]
                for i in enumerate(len(g2e)):
                    g2e[i] = g2e[i][idx][:, idx][:, :, idx][:, :, :, idx]
            else:
                h1e = h1e[idx][:, idx]
                g2e = g2e[idx][:, idx][:, :, idx][:, :, :, idx]
            if self.orb_sym is not None:
                self.orb_sym = bw.b.VectorUInt8(np.array(self.orb_sym)[idx])
                if self.ghamil is not None:
                    self.ghamil = bw.bs.GeneralHamiltonian(
                        self.vacuum, self.n_sites, self.orb_sym, self.heis_twos
                    )
        else:
            self.reorder_idx = None

        if para_type is None:
            para_type = ParallelTypes.SIJ

        if SymmetryTypes.SZ in bw.symm_type:
            if h1e is not None:
                h1e = list(h1e)
                h1e[0], _, ecore = self.parallelize_integrals(
                    para_type, h1e[0], None, ecore
                )
                h1e[1], _, _ = self.parallelize_integrals(para_type, h1e[1], None, 0)
            if g2e is not None:
                g2e = list(g2e)
                _, g2e[0], ecore = self.parallelize_integrals(
                    para_type, None, g2e[0], ecore
                )
                _, g2e[1], _ = self.parallelize_integrals(para_type, None, g2e[1], 0)
                _, g2e[2], _ = self.parallelize_integrals(para_type, None, g2e[2], 0)
        else:
            h1e, g2e, ecore = self.parallelize_integrals(para_type, h1e, g2e, ecore)

        if algo_type is not None and MPOAlgorithmTypes.Conventional in algo_type:
            fd = self.write_fcidump(h1e, g2e, ecore=ecore)
            return self.get_conventional_qc_mpo(fd, algo_type=algo_type, iprint=iprint)

        # build Hamiltonian expression
        b = self.expr_builder()

        if normal_order_ref is None:
            if SymmetryTypes.SU2 in bw.symm_type:
                if h1e is not None:
                    b.add_sum_term("(C+D)0", np.sqrt(2) * h1e)
                if g2e is not None:
                    b.add_sum_term("((C+(C+D)0)1+D)0", g2e.transpose(0, 2, 3, 1))
            elif SymmetryTypes.SZ in bw.symm_type:
                if h1e is not None:
                    b.add_sum_term("cd", h1e[0])
                    b.add_sum_term("CD", h1e[1])
                if g2e is not None:
                    b.add_sum_term("ccdd", 0.5 * g2e[0].transpose(0, 2, 3, 1))
                    b.add_sum_term("cCDd", 0.5 * g2e[1].transpose(0, 2, 3, 1))
                    b.add_sum_term("CcdD", 0.5 * g2e[1].transpose(2, 0, 1, 3))
                    b.add_sum_term("CCDD", 0.5 * g2e[2].transpose(0, 2, 3, 1))
            elif SymmetryTypes.SGF in bw.symm_type:
                if h1e is not None:
                    b.add_sum_term("CD", h1e)
                if g2e is not None:
                    b.add_sum_term("CCDD", 0.5 * g2e.transpose(0, 2, 3, 1))
        else:
            if SymmetryTypes.SU2 in bw.symm_type:
                h1es, g2es, ecore = NormalOrder.make_su2(
                    h1e, g2e, ecore, normal_order_ref
                )
            elif SymmetryTypes.SZ in bw.symm_type:
                h1es, g2es, ecore = NormalOrder.make_sz(
                    h1e, g2e, ecore, normal_order_ref
                )
            elif SymmetryTypes.SGF in bw.symm_type:
                h1es, g2es, ecore = NormalOrder.make_sgf(
                    h1e, g2e, ecore, normal_order_ref
                )

            if self.mpi is not None:
                ec_arr = np.array([ecore], dtype=float)
                self.mpi.reduce_sum(ec_arr, self.mpi.root)
                if self.mpi.rank == self.mpi.root:
                    ecore = ec_arr[0]
                else:
                    ecore = 0.0

            if post_integral_cutoff != 0:
                error = 0
                for k, v in h1es.items():
                    mask = np.abs(v) < post_integral_cutoff
                    error += np.sum(np.abs(v[mask]))
                    h1es[k][mask] = 0
                for k, v in g2es.items():
                    mask = np.abs(v) < post_integral_cutoff
                    error += np.sum(np.abs(v[mask]))
                    g2es[k][mask] = 0
                if iprint:
                    print("post integral cutoff error = ", error)

            for k, v in h1es.items():
                b.add_sum_term(k, v)
            for k, v in g2es.items():
                b.add_sum_term(k, v)

            if iprint:
                print("normal ordered ecore = ", ecore)

        b.add_const(ecore)
        bx = b.finalize()

        if iprint:
            if self.mpi is not None:
                for i in range(self.mpi.size):
                    self.mpi.barrier()
                    if i == self.mpi.rank:
                        print(
                            "rank = %5d mpo terms = %10d"
                            % (i, sum([len(x) for x in bx.data]))
                        )
                    self.mpi.barrier()
            else:
                print("mpo terms = %10d" % sum([len(x) for x in bx.data]))

        return self.get_mpo(
            bx,
            iprint=iprint,
            cutoff=cutoff,
            algo_type=algo_type,
            sum_mpo_mod=sum_mpo_mod,
            compute_accurate_svd_error=compute_accurate_svd_error,
            csvd_sparsity=csvd_sparsity,
            csvd_eps=csvd_eps,
            csvd_max_iter=csvd_max_iter,
            disjoint_levels=disjoint_levels,
            disjoint_all_blocks=disjoint_all_blocks,
            disjoint_multiplier=disjoint_multiplier,
        )

    def get_mpo(
        self,
        expr,
        iprint=0,
        cutoff=1e-14,
        left_vacuum=None,
        algo_type=None,
        sum_mpo_mod=-1,
        compute_accurate_svd_error=True,
        csvd_sparsity=0.0,
        csvd_eps=1e-10,
        csvd_max_iter=1000,
        disjoint_levels=None,
        disjoint_all_blocks=False,
        disjoint_multiplier=1.0,
    ):
        bw = self.bw
        if left_vacuum is None:
            left_vacuum = bw.SX.invalid
        if algo_type is None:
            algo_type = MPOAlgorithmTypes.FastBipartite
        algo_type = getattr(bw.b.MPOAlgorithmTypes, algo_type.name)
        mpo = bw.bs.GeneralMPO(self.ghamil, expr, algo_type, cutoff, -1, iprint)
        mpo.left_vacuum = left_vacuum
        mpo.sum_mpo_mod = sum_mpo_mod
        mpo.compute_accurate_svd_error = compute_accurate_svd_error
        mpo.csvd_sparsity = csvd_sparsity
        mpo.csvd_eps = csvd_eps
        mpo.csvd_max_iter = csvd_max_iter
        if disjoint_levels is not None:
            mpo.disjoint_levels = bw.VectorFP(disjoint_levels)
        mpo.disjoint_all_blocks = disjoint_all_blocks
        mpo.disjoint_multiplier = disjoint_multiplier
        mpo.build()
        mpo = bw.bs.SimplifiedMPO(mpo, bw.bs.Rule(), False, False)
        mpo = bw.bs.IdentityAddedMPO(mpo)
        if self.mpi:
            mpo = bw.bs.ParallelMPO(mpo, self.prule)
        return mpo

    def get_spin_square_mpo(self, iprint=1):
        import numpy as np

        bw = self.bw
        b = self.expr_builder()

        if SymmetryTypes.SU2 in bw.symm_type:
            ix2 = np.mgrid[: self.n_sites, : self.n_sites].reshape((2, -1))
            if self.reorder_idx is not None:
                ridx = np.argsort(self.reorder_idx)
                ix2 = np.array(ridx)[ix2]
            b.add_terms(
                "((C+D)2+(C+D)2)0",
                -np.sqrt(3) / 2 * np.ones(ix2.shape[1]),
                np.array([ix2[0], ix2[0], ix2[1], ix2[1]]).T,
            )
        elif SymmetryTypes.SZ in bw.symm_type:
            ix1 = np.mgrid[: self.n_sites].flatten()
            ix2 = np.mgrid[: self.n_sites, : self.n_sites].reshape((2, -1))
            if self.reorder_idx is not None:
                ridx = np.argsort(self.reorder_idx)
                ix1 = np.array(ridx)[ix1]
                ix2 = np.array(ridx)[ix2]
            b.add_terms("cd", 0.75 * np.ones(ix1.shape[0]), np.array([ix1, ix1]).T)
            b.add_terms("CD", 0.75 * np.ones(ix1.shape[0]), np.array([ix1, ix1]).T)
            b.add_terms(
                "ccdd",
                0.25 * np.ones(ix2.shape[1]),
                np.array([ix2[0], ix2[1], ix2[1], ix2[0]]).T,
            )
            b.add_terms(
                "cCDd",
                -0.25 * np.ones(ix2.shape[1]),
                np.array([ix2[0], ix2[1], ix2[1], ix2[0]]).T,
            )
            b.add_terms(
                "CcdD",
                -0.25 * np.ones(ix2.shape[1]),
                np.array([ix2[0], ix2[1], ix2[1], ix2[0]]).T,
            )
            b.add_terms(
                "CCDD",
                0.25 * np.ones(ix2.shape[1]),
                np.array([ix2[0], ix2[1], ix2[1], ix2[0]]).T,
            )
            b.add_terms(
                "cCDd",
                -0.5 * np.ones(ix2.shape[1]),
                np.array([ix2[0], ix2[1], ix2[0], ix2[1]]).T,
            )
            b.add_terms(
                "CcdD",
                -0.5 * np.ones(ix2.shape[1]),
                np.array([ix2[1], ix2[0], ix2[1], ix2[0]]).T,
            )
        elif SymmetryTypes.SGF in bw.symm_type:
            ixa1 = np.mgrid[0 : self.n_sites : 2].flatten()
            ixb1 = np.mgrid[1 : self.n_sites : 2].flatten()
            ixaa2 = np.mgrid[0 : self.n_sites : 2, 0 : self.n_sites : 2].reshape(
                (2, -1)
            )
            ixab2 = np.mgrid[0 : self.n_sites : 2, 1 : self.n_sites : 2].reshape(
                (2, -1)
            )
            ixba2 = np.mgrid[1 : self.n_sites : 2, 0 : self.n_sites : 2].reshape(
                (2, -1)
            )
            ixbb2 = np.mgrid[1 : self.n_sites : 2, 1 : self.n_sites : 2].reshape(
                (2, -1)
            )
            if self.reorder_idx is not None:
                ridx = np.argsort(self.reorder_idx)
                ixa1 = np.array(ridx)[ixa1]
                ixb1 = np.array(ridx)[ixb1]
                ixaa2 = np.array(ridx)[ixaa2]
                ixab2 = np.array(ridx)[ixab2]
                ixba2 = np.array(ridx)[ixba2]
                ixbb2 = np.array(ridx)[ixbb2]
            b.add_terms("CD", 0.75 * np.ones(ixa1.shape[0]), np.array([ixa1, ixa1]).T)
            b.add_terms("CD", 0.75 * np.ones(ixb1.shape[0]), np.array([ixb1, ixb1]).T)
            b.add_terms(
                "CCDD",
                0.25 * np.ones(ixaa2.shape[1]),
                np.array([ixaa2[0], ixaa2[1], ixaa2[1], ixaa2[0]]).T,
            )
            b.add_terms(
                "CCDD",
                -0.25 * np.ones(ixab2.shape[1]),
                np.array([ixab2[0], ixab2[1], ixab2[1], ixab2[0]]).T,
            )
            b.add_terms(
                "CCDD",
                -0.25 * np.ones(ixba2.shape[1]),
                np.array([ixba2[0], ixba2[1], ixba2[1], ixba2[0]]).T,
            )
            b.add_terms(
                "CCDD",
                0.25 * np.ones(ixbb2.shape[1]),
                np.array([ixbb2[0], ixbb2[1], ixbb2[1], ixbb2[0]]).T,
            )
            b.add_terms(
                "CCDD",
                -0.5 * np.ones(ixab2.shape[1]),
                np.array([ixab2[0], ixab2[1], ixba2[0], ixba2[1]]).T,
            )
            b.add_terms(
                "CCDD",
                -0.5 * np.ones(ixba2.shape[1]),
                np.array([ixab2[1], ixab2[0], ixba2[1], ixba2[0]]).T,
            )

        if self.mpi is not None:
            b.iscale(1.0 / self.mpi.size)

        bx = b.finalize()
        return self.get_mpo(bx, iprint)

    def orbital_reordering(self, h1e, g2e):
        bw = self.bw
        import numpy as np

        xmat = np.abs(np.einsum("ijji->ij", g2e, optimize=True))
        kmat = np.abs(h1e) * 1e-7 + xmat
        kmat = bw.b.VectorDouble(kmat.flatten())
        idx = bw.b.OrbitalOrdering.fiedler(len(h1e), kmat)
        return np.array(idx)

    def dmrg(
        self,
        mpo,
        ket,
        n_sweeps=10,
        tol=1e-8,
        bond_dims=None,
        noises=None,
        thrds=None,
        iprint=0,
        dav_type=None,
        cutoff=1e-20,
        dav_max_iter=4000,
        proj_mpss=None,
        proj_weights=None,
        store_wfn_spectra=True,
    ):
        bw = self.bw
        if bond_dims is None:
            bond_dims = [ket.info.bond_dim]
        if noises is None:
            noises = [1e-5] * 5 + [0]
        if thrds is None:
            if SymmetryTypes.SP not in bw.symm_type:
                thrds = [1e-6] * 4 + [1e-7] * 1
            else:
                thrds = [1e-5] * 4 + [5e-6] * 1
        if dav_type is not None and "LeftEigen" in dav_type:
            bra = ket.deep_copy("BRA")
        else:
            bra = ket
        me = bw.bs.MovingEnvironment(mpo, bra, ket, "DMRG")
        me.delayed_contraction = bw.b.OpNamesSet.normal_ops()
        me.cached_contraction = True
        dmrg = bw.bs.DMRG(me, bw.b.VectorUBond(bond_dims), bw.VectorFP(noises))

        if proj_mpss is not None:
            assert proj_weights is not None
            assert len(proj_weights) == len(proj_mpss)
            dmrg.projection_weights = bw.VectorFP(proj_weights)
            dmrg.ext_mpss = bw.bs.VectorMPS(proj_mpss)
            impo = self.get_identity_mpo()
            for ext_mps in dmrg.ext_mpss:
                self.align_mps_center(ext_mps, ket)
                ext_me = bw.bs.MovingEnvironment(
                    impo, ket, ext_mps, "PJ" + ext_mps.info.tag
                )
                ext_me.delayed_contraction = bw.b.OpNamesSet.normal_ops()
                ext_me.init_environments(iprint >= 2)
                dmrg.ext_mes.append(ext_me)

        if dav_type is not None:
            dmrg.davidson_type = getattr(bw.b.DavidsonTypes, dav_type)
        dmrg.noise_type = bw.b.NoiseTypes.ReducedPerturbative
        dmrg.davidson_conv_thrds = bw.VectorFP(thrds)
        dmrg.davidson_max_iter = dav_max_iter + 100
        dmrg.davidson_soft_max_iter = dav_max_iter
        dmrg.store_wfn_spectra = store_wfn_spectra
        dmrg.iprint = iprint
        dmrg.cutoff = cutoff
        dmrg.trunc_type = dmrg.trunc_type | bw.b.TruncationTypes.RealDensityMatrix
        self._dmrg = dmrg
        if n_sweeps == -1:
            return None
        me.init_environments(iprint >= 2)
        ener = dmrg.solve(n_sweeps, ket.center == 0, tol)
        ket.info.bond_dim = max(ket.info.bond_dim, bond_dims[-1])
        if isinstance(ket, bw.bs.MultiMPS):
            ener = list(dmrg.energies[-1])
        if self.mpi is not None:
            self.mpi.barrier()
        return ener

    def get_dmrg_results(self):
        import numpy as np

        energies = np.array(self._dmrg.energies)
        dws = np.array(self._dmrg.discarded_weights)
        bond_dims = np.array(self._dmrg.bond_dims)[: len(energies)]
        return bond_dims, dws, energies

    def get_bipartite_entanglement(self):
        import numpy as np

        bip_ent = np.zeros(len(self._dmrg.sweep_wfn_spectra), dtype=np.float64)
        for ix, x in enumerate(self._dmrg.sweep_wfn_spectra):
            ldsq = np.array(x, dtype=np.float128) ** 2
            ldsq = ldsq[ldsq != 0]
            bip_ent[ix] = float(np.sum(-ldsq * np.log(ldsq)))
        return bip_ent

    def get_npdm(self, ket, pdm_type=1, bra=None, soc=False, site_type=1, iprint=0):
        bw = self.bw
        import numpy as np

        if self.mpi is not None:
            self.mpi.barrier()

        mket = ket.deep_copy("PDM-KET@TMP")
        mpss = [mket]
        if bra is not None and bra != ket:
            mbra = bra.deep_copy("PDM-BRA@TMP")
            mpss.append(mbra)
        else:
            mbra = mket
        for mps in mpss:
            if mps.dot == 2 and site_type != 2:
                mps.dot = 1
                if mps.center == mps.n_sites - 2:
                    mps.center = mps.n_sites - 1
                    mps.canonical_form = mps.canonical_form[:-1] + "S"
                elif mps.center == 0:
                    mps.canonical_form = "K" + mps.canonical_form[1:]
                else:
                    assert False

            if self.mpi is not None:
                self.mpi.barrier()

            mps.load_mutable()
            mps.info.bond_dim = max(
                mps.info.bond_dim, mps.info.get_max_bond_dimension()
            )

        self.align_mps_center(mbra, mket)
        hamil = bw.bs.HamiltonianQC(
            self.vacuum, self.n_sites, self.orb_sym, bw.bx.FCIDUMP()
        )
        if pdm_type == 1:
            pmpo = bw.bs.PDM1MPOQC(hamil, 1 if soc else 0)
        elif pdm_type == 2:
            pmpo = bw.bs.PDM2MPOQC(hamil)
        else:
            raise NotImplementedError()
        if mbra == mket:
            pmpo = bw.bs.SimplifiedMPO(pmpo, bw.bs.RuleQC())
        else:
            pmpo = bw.bs.SimplifiedMPO(pmpo, bw.bs.NoTransposeRule(bw.bs.RuleQC()))
        if self.mpi:
            if pdm_type == 1:
                prule = bw.bs.ParallelRulePDM1QC(self.mpi)
            elif pdm_type == 2:
                prule = bw.bs.ParallelRulePDM2QC(self.mpi)
            pmpo = bw.bs.ParallelMPO(pmpo, prule)

        if soc:
            mb_lv = mbra.info.left_dims_fci[0].quanta[0]
            ml_lv = mket.info.left_dims_fci[0].quanta[0]
            if mb_lv != ml_lv:
                raise RuntimeError(
                    "SOC 1PDM cannot be done with singlet_embedding for mpss"
                    + " with different spins! Please consider setting "
                    + "singlet_embedding=False in DMRGDriver.initialize_system(...)."
                )

        pme = bw.bs.MovingEnvironment(pmpo, mbra, mket, "NPDM")
        pme.init_environments(iprint >= 2)
        pme.cached_contraction = True
        expect = bw.bs.Expect(pme, mbra.info.bond_dim, mket.info.bond_dim)
        if site_type == 0:
            expect.zero_dot_algo = True
        # expect.algo_type = bw.b.ExpectationAlgorithmTypes.Normal
        expect.iprint = iprint
        expect.solve(True, mket.center == 0)

        if pdm_type == 1:
            if SymmetryTypes.SZ in bw.symm_type:
                dmr = expect.get_1pdm(self.n_sites)
                dm = np.array(dmr).copy()
                dmr.deallocate()
                dm = dm.reshape((self.n_sites, 2, self.n_sites, 2))
                dm = np.transpose(dm, (0, 2, 1, 3))
                dm = np.concatenate(
                    [dm[None, :, :, 0, 0], dm[None, :, :, 1, 1]], axis=0
                )
            elif SymmetryTypes.SU2 in bw.symm_type:
                dmr = expect.get_1pdm_spatial(self.n_sites)
                dm = np.array(dmr).copy()
                dmr.deallocate()
                if soc:
                    if SymmetryTypes.SU2 in bw.symm_type:
                        if hasattr(mbra.info, "targets"):
                            qsbra = mbra.info.targets[0].twos
                        else:
                            qsbra = mbra.info.target.twos
                        # fix different Wigner–Eckart theorem convention
                        dm *= np.sqrt(qsbra + 1)
                    dm = dm / np.sqrt(2)
            else:
                dmr = expect.get_1pdm(self.n_sites)
                dm = np.array(dmr).copy()
                dmr.deallocate()

            if self.reorder_idx is not None:
                rev_idx = np.argsort(self.reorder_idx)
                dm = dm[..., rev_idx, :][..., :, rev_idx]
        elif pdm_type == 2:
            dmr = expect.get_2pdm(self.n_sites)
            dm = np.array(dmr, copy=True)
            dm = dm.reshape(
                (self.n_sites, 2, self.n_sites, 2, self.n_sites, 2, self.n_sites, 2)
            )
            dm = np.transpose(dm, (0, 2, 4, 6, 1, 3, 5, 7))
            dm = np.concatenate(
                [
                    dm[None, :, :, :, :, 0, 0, 0, 0],
                    dm[None, :, :, :, :, 0, 1, 1, 0],
                    dm[None, :, :, :, :, 1, 1, 1, 1],
                ],
                axis=0,
            )
            if self.reorder_idx is not None:
                dm[:, :, :, :, :] = dm[:, rev_idx, :, :, :][:, :, rev_idx, :, :][
                    :, :, :, rev_idx, :
                ][:, :, :, :, rev_idx]

        if self.mpi is not None:
            self.mpi.barrier()
        return dm

    def get_1pdm(self, ket, *args, **kwargs):
        return self.get_npdm(ket, pdm_type=1, *args, **kwargs)

    def get_2pdm(self, ket, *args, **kwargs):
        return self.get_npdm(ket, pdm_type=2, *args, **kwargs)

    def get_trans_1pdm(self, bra, ket, *args, **kwargs):
        return self.get_npdm(ket, pdm_type=1, bra=bra, *args, **kwargs)

    def get_trans_2pdm(self, bra, ket, *args, **kwargs):
        return self.get_npdm(ket, pdm_type=2, bra=bra, *args, **kwargs)

    def align_mps_center(self, ket, ref):
        if self.mpi is not None:
            self.mpi.barrier()
        ket.info.bond_dim = max(ket.info.bond_dim, ket.info.get_max_bond_dimension())
        if ket.center != ref.center:
            if ref.center == 0:
                if ket.dot == 2:
                    ket.center += 1
                    ket.canonical_form = ket.canonical_form[:-1] + "S"
                while ket.center != 0:
                    ket.move_left(self.ghamil.opf.cg, self.prule)
            else:
                ket.canonical_form = "K" + ket.canonical_form[1:]
                while ket.center != ket.n_sites - 1:
                    ket.move_right(self.ghamil.opf.cg, self.prule)
                if ket.dot == 2:
                    ket.center -= 1
            if self.mpi is not None:
                self.mpi.barrier()
            ket.save_data()
            ket.info.save_data(self.scratch + "/%s-mps_info.bin" % ket.info.tag)
        if self.mpi is not None:
            self.mpi.barrier()

    def adjust_mps(self, ket, dot=1):
        bw = self.bw
        if ket.center == 0 and dot == 2:
            if self.mpi is not None:
                self.mpi.barrier()
            if ket.canonical_form[ket.center] in "ST":
                ket.flip_fused_form(ket.center, self.ghamil.opf.cg, self.prule)
            ket.save_data()
            forward = True
            if self.mpi is not None:
                self.mpi.barrier()
            ket.load_mutable()
            ket.info.load_mutable()
            if self.mpi is not None:
                self.mpi.barrier()

        ket.dot = dot
        forward = ket.center == 0
        if (
            ket.canonical_form[ket.center] == "L"
            and ket.center != ket.n_sites - ket.dot
        ):
            ket.center += 1
            forward = True
        elif (
            ket.canonical_form[ket.center] == "C"
            or ket.canonical_form[ket.center] == "M"
        ) and ket.center != 0:
            ket.center -= 1
            forward = False
        if ket.canonical_form[ket.center] == "M" and not isinstance(
            ket, bw.bs.MultiMPS
        ):
            ket.canonical_form = (
                ket.canonical_form[: ket.center]
                + "C"
                + ket.canonical_form[ket.center + 1 :]
            )
        if ket.canonical_form[-1] == "M" and not isinstance(ket, bw.bs.MultiMPS):
            ket.canonical_form = ket.canonical_form[:-1] + "C"
        if dot == 1:
            if ket.canonical_form[0] == "C" and ket.canonical_form[1] == "R":
                ket.canonical_form = "K" + ket.canonical_form[1:]
            elif ket.canonical_form[-1] == "C" and ket.canonical_form[-2] == "L":
                ket.canonical_form = ket.canonical_form[:-1] + "S"
                ket.center = ket.n_sites - 1
            elif ket.center == ket.n_sites - 2 and ket.canonical_form[-2] == "L":
                ket.center = ket.n_sites - 1
            if ket.canonical_form[0] == "M" and ket.canonical_form[1] == "R":
                ket.canonical_form = "J" + ket.canonical_form[1:]
            elif ket.canonical_form[-1] == "M" and ket.canonical_form[-2] == "L":
                ket.canonical_form = ket.canonical_form[:-1] + "T"
                ket.center = ket.n_sites - 1
            elif ket.center == ket.n_sites - 2 and ket.canonical_form[-2] == "L":
                ket.center = ket.n_sites - 1

        ket.save_data()
        if self.mpi is not None:
            self.mpi.barrier()
        return ket, forward

    def split_mps(self, ket, iroot, tag):
        bw = self.bw
        if self.mpi is not None:
            self.mpi.barrier()
        assert isinstance(ket, bw.bs.MultiMPS)
        iket = ket.extract(iroot, tag + "@TMP")
        if self.mpi is not None:
            self.mpi.barrier()
        if len(iket.info.targets) == 1:
            iket = iket.make_single(tag)
        if self.mpi is not None:
            self.mpi.barrier()
        iket = self.adjust_mps(iket)[0]
        iket.info.save_data(self.scratch + "/%s-mps_info.bin" % tag)
        return iket

    def multiply(
        self,
        bra,
        mpo,
        ket,
        n_sweeps=10,
        tol=1e-8,
        bond_dims=None,
        bra_bond_dims=None,
        cutoff=1e-24,
        iprint=0,
    ):
        bw = self.bw
        if bra.info.tag == ket.info.tag:
            raise RuntimeError("Same tag for bra and ket!!")
        if bond_dims is None:
            bond_dims = [ket.info.bond_dim]
        if bra_bond_dims is None:
            bra_bond_dims = [bra.info.bond_dim]
        self.align_mps_center(bra, ket)
        me = bw.bs.MovingEnvironment(mpo, bra, ket, "MULT")
        me.delayed_contraction = bw.b.OpNamesSet.normal_ops()
        me.cached_contraction = True
        me.init_environments(iprint >= 2)
        cps = bw.bs.Linear(
            me, bw.b.VectorUBond(bra_bond_dims), bw.b.VectorUBond(bond_dims)
        )
        cps.iprint = iprint
        cps.cutoff = cutoff
        norm = cps.solve(n_sweeps, ket.center == 0, tol)
        if self.mpi is not None:
            self.mpi.barrier()
        return norm

    def expectation(self, bra, mpo, ket, iprint=0):
        bw = self.bw
        bond_dim = max(bra.info.bond_dim, ket.info.bond_dim)
        self.align_mps_center(bra, ket)
        me = bw.bs.MovingEnvironment(mpo, bra, ket, "EXPT")
        me.delayed_contraction = bw.b.OpNamesSet.normal_ops()
        me.cached_contraction = True
        me.init_environments(iprint >= 2)
        expect = bw.bs.Expect(me, bond_dim, bond_dim)
        expect.iprint = iprint
        ex = expect.solve(False, ket.center != 0)
        if self.mpi is not None:
            self.mpi.barrier()
        return ex

    def fix_restarting_mps(self, mps):
        cg = self.ghamil.opf.cg
        if (
            mps.canonical_form[mps.center] == "L"
            and mps.center != mps.n_sites - mps.dot
        ):
            mps.center += 1
            if mps.canonical_form[mps.center] in "ST" and mps.dot == 2:
                if self.mpi is not None:
                    self.mpi.barrier()
                mps.flip_fused_form(mps.center, cg, self.prule)
                mps.save_data()
                if self.mpi is not None:
                    self.mpi.barrier()
                mps.load_mutable()
                mps.info.load_mutable()
                if self.mpi is not None:
                    self.mpi.barrier()
        elif mps.canonical_form[mps.center] in "CMKJST" and mps.center != 0:
            if mps.canonical_form[mps.center] in "KJ" and mps.dot == 2:
                if self.mpi is not None:
                    self.mpi.barrier()
                mps.flip_fused_form(mps.center, cg, self.prule)
                mps.save_data()
                if self.mpi is not None:
                    self.mpi.barrier()
                mps.load_mutable()
                mps.info.load_mutable()
                if self.mpi is not None:
                    self.mpi.barrier()
            if (
                not mps.canonical_form[mps.center : mps.center + 2] == "CC"
                and mps.dot == 2
            ):
                mps.center -= 1
        elif mps.center == mps.n_sites - 1 and mps.dot == 2:
            if self.mpi is not None:
                self.mpi.barrier()
            if mps.canonical_form[mps.center] in "KJ":
                mps.flip_fused_form(mps.center, cg, self.prule)
            mps.center = mps.n_sites - 2
            mps.save_data()
            if self.mpi is not None:
                self.mpi.barrier()
            mps.load_mutable()
            mps.info.load_mutable()
            if self.mpi is not None:
                self.mpi.barrier()
        elif mps.center == 0 and mps.dot == 2:
            if self.mpi is not None:
                self.mpi.barrier()
            if mps.canonical_form[mps.center] in "ST":
                mps.flip_fused_form(mps.center, cg, self.prule)
            mps.save_data()
            if self.mpi is not None:
                self.mpi.barrier()
            mps.load_mutable()
            mps.info.load_mutable()
            if self.mpi is not None:
                self.mpi.barrier()

    def load_mps(self, tag, nroots=1):
        import os

        bw = self.bw
        mps_info = bw.brs.MPSInfo(0) if nroots == 1 else bw.brs.MultiMPSInfo(0)
        if os.path.isfile(self.scratch + "/%s-mps_info.bin" % tag):
            mps_info.load_data(self.scratch + "/%s-mps_info.bin" % tag)
        else:
            mps_info.load_data(self.scratch + "/mps_info.bin")
        assert mps_info.tag == tag
        mps_info.load_mutable()
        mps_info.bond_dim = max(mps_info.bond_dim, mps_info.get_max_bond_dimension())
        mps = bw.bs.MPS(mps_info) if nroots == 1 else bw.bs.MultiMPS(mps_info)
        mps.load_data()
        mps.load_mutable()
        self.fix_restarting_mps(mps)
        return mps

    def mps_change_singlet_embedding(self, mps, tag, forward, left_vacuum=None):
        cp_mps = mps.deep_copy(tag)
        while cp_mps.center > 0:
            cp_mps.move_left(self.ghamil.opf.cg, self.prule)
        if forward:
            if left_vacuum is None:
                left_vacuum = self.bw.SX.invalid
            cp_mps.to_singlet_embedding_wfn(self.ghamil.opf.cg, left_vacuum, self.prule)
        else:
            cp_mps.from_singlet_embedding_wfn(self.ghamil.opf.cg, self.prule)
        cp_mps.save_data()
        cp_mps.info.save_data(self.scratch + "/%s-mps_info.bin" % tag)
        if self.mpi is not None:
            self.mpi.barrier()
        return cp_mps

    def mps_change_to_singlet_embedding(self, mps, tag):
        return self.mps_change_singlet_embedding(mps, tag, forward=True)

    def mps_change_from_singlet_embedding(self, mps, tag):
        return self.mps_change_singlet_embedding(mps, tag, forward=False)

    def mps_change_precision(self, mps, tag):
        bw = self.bw
        assert tag != mps.info.tag
        if SymmetryTypes.SP in bw.symm_type:
            r = bw.bs.trans_mps_to_double(mps, tag)
        else:
            r = bw.bs.trans_mps_to_float(mps, tag)
        r.info.save_data(self.scratch + "/%s-mps_info.bin" % tag)
        return r

    def get_random_mps(
        self,
        tag,
        bond_dim=500,
        center=0,
        dot=2,
        target=None,
        nroots=1,
        occs=None,
        full_fci=True,
        left_vacuum=None,
    ):
        bw = self.bw
        if target is None:
            target = self.target
        if left_vacuum is None:
            left_vacuum = self.left_vacuum
        if nroots == 1:
            mps_info = bw.brs.MPSInfo(
                self.n_sites, self.vacuum, target, self.ghamil.basis
            )
            mps = bw.bs.MPS(self.n_sites, center, dot)
        else:
            targets = bw.VectorSX([target]) if isinstance(target, bw.SX) else target
            mps_info = bw.brs.MultiMPSInfo(
                self.n_sites, self.vacuum, targets, self.ghamil.basis
            )
            mps = bw.bs.MultiMPS(self.n_sites, center, dot, nroots)
        mps_info.tag = tag
        if full_fci:
            mps_info.set_bond_dimension_full_fci(left_vacuum, self.vacuum)
        else:
            mps_info.set_bond_dimension_fci(left_vacuum, self.vacuum)
        if occs is not None:
            mps_info.set_bond_dimension_using_occ(bond_dim, bw.b.VectorDouble(occs))
        else:
            mps_info.set_bond_dimension(bond_dim)
        mps_info.bond_dim = bond_dim
        mps.initialize(mps_info)
        mps.random_canonicalize()
        if nroots == 1:
            mps.tensors[mps.center].normalize()
        else:
            for xwfn in mps.wfns:
                xwfn.normalize()
        mps.save_mutable()
        mps_info.save_mutable()
        mps.save_data()
        mps_info.save_data(self.scratch + "/%s-mps_info.bin" % tag)
        return mps

    def expr_builder(self):
        return ExprBuilder(self.bw)

    def finalize(self):
        bw = self.bw
        bw.b.Global.frame = None


class SOCDMRGDriver(DMRGDriver):
    def __init__(
        self,
        stack_mem=1 << 30,
        scratch="./nodex",
        restart_dir=None,
        n_threads=None,
        symm_type=SymmetryTypes.SU2,
        mpi=None,
    ):
        super().__init__(
            stack_mem=stack_mem,
            scratch=scratch,
            restart_dir=restart_dir,
            n_threads=n_threads,
            symm_type=symm_type,
            mpi=mpi,
        )

    def hybrid_mpo_dmrg(
        self, mpo, mpo_cpx, ket, n_sweeps=10, iprint=0, tol=1e-8, **kwargs
    ):
        bw = self.bw
        assert ket.nroots % 2 == 0
        super().dmrg(mpo, ket, n_sweeps=-1, iprint=iprint, tol=tol, **kwargs)
        self._dmrg.me.cached_contraction = False
        self._dmrg.me.delayed_contraction = bw.b.OpNamesSet()
        self._dmrg.me.init_environments(iprint >= 2)
        cpx_me = bw.bcs.MovingEnvironmentX(mpo_cpx, ket, ket, "DMRG-CPX")
        cpx_me.cached_contraction = False
        cpx_me.init_environments(iprint >= 2)
        self._dmrg.cpx_me = cpx_me
        ener = self._dmrg.solve(n_sweeps, ket.center == 0, tol)
        ket.info.bond_dim = max(ket.info.bond_dim, self._dmrg.bond_dims[-1])
        if isinstance(ket, bw.bs.MultiMPS):
            ener = list(self._dmrg.energies[-1])
        if self.mpi is not None:
            self.mpi.barrier()
        return ener

    def soc_two_step(self, energies, twoss, pdms_dict, hsomo, iprint=1):
        au2cm = 219474.631115585274529
        import numpy as np

        assert len(twoss) == len(energies)

        xnroots = [len(x) for x in energies]
        eners = []
        xtwos = []
        for ix in range(len(energies)):
            eners += energies[ix]
            xtwos += [twoss[ix]] * xnroots[ix]
        eners = np.array(eners)

        pdm0 = pdms_dict[(0, 0)]
        assert pdm0.ndim == 2 and pdm0.shape[0] == pdm0.shape[1]
        ncas = pdm0.shape[0]

        pdms = np.zeros((len(eners), len(eners), ncas, ncas))
        for ist in range(len(eners)):
            for jst in range(len(eners)):
                if ist >= jst and abs(xtwos[ist] - xtwos[jst]) <= 2:
                    pdms[ist, jst] = pdms_dict[(ist, jst)]

        if iprint and (self.mpi is None or self.mpi.rank == self.mpi.root):
            print("HSO.SHAPE = ", hsomo.shape)
            print("PDMS.SHAPE = ", pdms.shape)
        thrds = 29.0  # cm-1
        n_mstates = 0
        for ix, iis in enumerate(twoss):
            n_mstates += (iis + 1) * xnroots[ix]
        hsiso = np.zeros((n_mstates, n_mstates), dtype=complex)
        hdiag = np.zeros((n_mstates,), dtype=complex)

        # separate T^1 to T^1_(-1,0,1)
        def spin_proj(cg, pdm, tjo, tjb, tjk):
            nmo = pdm.shape[0]
            ppdm = np.zeros((tjb + 1, tjk + 1, tjo + 1, nmo, nmo))
            for ibra in range(tjb + 1):
                for iket in range(tjk + 1):
                    for iop in range(tjo + 1):
                        tmb = -tjb + 2 * ibra
                        tmk = -tjk + 2 * iket
                        tmo = -tjo + 2 * iop
                        factor = (-1) ** ((tjb - tmb) // 2) * cg.wigner_3j(
                            tjb, tjo, tjk, -tmb, tmo, tmk
                        )
                        if factor != 0:
                            ppdm[ibra, iket, iop] = pdm * factor
            return ppdm

        # from T^1_(-1,0,1) to Tx, Ty, Tz
        def xyz_proj(ppdm):
            xpdm = np.zeros(ppdm.shape, dtype=complex)
            xpdm[:, :, 0] = (0.5 + 0j) * (ppdm[:, :, 0] - ppdm[:, :, 2])
            xpdm[:, :, 1] = (0.5j + 0) * (ppdm[:, :, 0] + ppdm[:, :, 2])
            xpdm[:, :, 2] = (np.sqrt(0.5) + 0j) * ppdm[:, :, 1]
            return xpdm

        cg = self.ghamil.opf.cg

        qls = []
        imb = 0
        for ibra in range(len(pdms)):
            imk = 0
            tjb = xtwos[ibra]
            for iket in range(len(pdms)):
                tjk = xtwos[iket]
                if ibra >= iket:
                    pdm = pdms[ibra, iket]
                    xpdm = xyz_proj(spin_proj(cg, pdm, 2, tjb, tjk))
                    for ibm in range(xpdm.shape[0]):
                        for ikm in range(xpdm.shape[1]):
                            somat = np.einsum("rij,rij->", xpdm[ibm, ikm], hsomo)
                            hsiso[ibm + imb, ikm + imk] = somat
                            somat *= au2cm
                            if iprint and (
                                self.mpi is None or self.mpi.rank == self.mpi.root
                            ):
                                if abs(somat) > thrds:
                                    print(
                                        (
                                            "I1 = %4d (E1 = %15.8f) S1 = %4.1f MS1 = %4.1f "
                                            + "I2 = %4d (E2 = %15.8f) S2 = %4.1f MS2 = %4.1f Re = %9.3f Im = %9.3f"
                                        )
                                        % (
                                            ibra,
                                            eners[ibra],
                                            tjb / 2,
                                            -tjb / 2 + ibm,
                                            iket,
                                            eners[iket],
                                            tjk / 2,
                                            -tjk / 2 + ikm,
                                            somat.real,
                                            somat.imag,
                                        )
                                    )
                imk += tjk + 1
            for ibm in range(tjb + 1):
                qls.append((ibra, eners[ibra], tjb / 2, -tjb / 2 + ibm))
            hdiag[imb : imb + tjb + 1] = eners[ibra]
            imb += tjb + 1

        for i in range(len(hsiso)):
            for j in range(len(hsiso)):
                if i >= j:
                    hsiso[j, i] = hsiso[i, j].conj()

        self._hsiso = hsiso * au2cm

        symm_err = np.linalg.norm(np.abs(hsiso - hsiso.T.conj()))
        if iprint and (self.mpi is None or self.mpi.rank == self.mpi.root):
            print("SYMM Error (should be small) = ", symm_err)
        assert symm_err < 1e-10
        hfull = hsiso + np.diag(hdiag)
        heig, hvec = np.linalg.eigh(hfull)
        if iprint and (self.mpi is None or self.mpi.rank == self.mpi.root):
            print("Total energies including SO-coupling:\n")
        xhdiag = np.zeros_like(heig)

        for i in range(len(heig)):
            shvec = np.zeros(len(eners))
            ssq = 0
            imb = 0
            for ibra in range(len(eners)):
                tjb = xtwos[ibra]
                shvec[ibra] = np.linalg.norm(hvec[imb : imb + tjb + 1, i]) ** 2
                ssq += shvec[ibra] * (tjb + 2) * tjb / 4
                imb += tjb + 1
            assert abs(np.sum(shvec) - 1) < 1e-7
            iv = np.argmax(np.abs(shvec))
            xhdiag[i] = eners[iv]
            if iprint and (self.mpi is None or self.mpi.rank == self.mpi.root):
                print(
                    " State %4d Total energy: %15.8f <S^2>: %12.6f | largest |coeff|**2 %10.6f from I = %4d E = %15.8f S = %4.1f"
                    % (i, heig[i], ssq, shvec[iv], iv, eners[iv], xtwos[iv] / 2)
                )

        return heig


class NormalOrder:
    @staticmethod
    def def_ix(cidx):
        import numpy as np

        def ix(x):
            p = {"I": cidx, "E": ~cidx}
            if len(x) == 2:
                return np.outer(p[x[0]], p[x[1]])
            else:
                return np.outer(np.outer(np.outer(p[x[0]], p[x[1]]), p[x[2]]), p[x[3]])

        return ix

    @staticmethod
    def def_gctr(cidx, h1e, g2e):
        import numpy as np

        def gctr(x):
            v = g2e if len(x.split("->")[0]) == 4 else h1e
            for ig, g in enumerate(x.split("->")[0]):
                if x.split("->")[0].count(g) == 2:
                    v = v[(slice(None),) * ig + (cidx,)]
            return np.einsum(x, v, optimize=True)

        return gctr

    @staticmethod
    def def_gctr_sz(cidx, h1e, g2e):
        import numpy as np

        def gctr(i, x):
            v = g2e[i] if len(x.split("->")[0]) == 4 else h1e[i]
            for ig, g in enumerate(x.split("->")[0]):
                if x.split("->")[0].count(g) == 2:
                    v = v[(slice(None),) * ig + (cidx,)]
            return np.einsum(x, v, optimize=True)

        return gctr

    @staticmethod
    def make_su2(h1e, g2e, const_e, cidx):
        import numpy as np

        ix = NormalOrder.def_ix(cidx)
        gctr = NormalOrder.def_gctr(cidx, h1e, g2e)

        h1es = {k: np.zeros_like(h1e) for k in ("CD", "DC")}
        g2es = {k: np.zeros_like(g2e) for k in ("CCDD", "CDCD", "DDCC", "DCDC")}
        const_es = const_e

        const_es += 2.0 * gctr("qq->")
        const_es += 2.0 * gctr("rrss->")
        const_es -= 1.0 * gctr("srrs->")

        np.putmask(h1es["CD"], ix("EI"), h1es["CD"] + gctr("pq->pq"))
        np.putmask(h1es["CD"], ix("EE"), h1es["CD"] + gctr("pq->pq"))
        np.putmask(h1es["DC"], ix("II"), h1es["DC"] + gctr("pq->qp"))
        np.putmask(h1es["CD"], ix("IE"), h1es["CD"] + gctr("pq->pq"))

        np.putmask(h1es["CD"], ix("EI"), h1es["CD"] - 1.0 * gctr("prrs->ps"))
        np.putmask(h1es["CD"], ix("EI"), h1es["CD"] + 2.0 * gctr("prss->pr"))
        np.putmask(h1es["CD"], ix("EE"), h1es["CD"] + 2.0 * gctr("prss->pr"))
        np.putmask(h1es["DC"], ix("II"), h1es["DC"] - 1.0 * gctr("prrs->sp"))
        np.putmask(h1es["DC"], ix("II"), h1es["DC"] + 2.0 * gctr("prss->rp"))
        np.putmask(h1es["CD"], ix("IE"), h1es["CD"] + 2.0 * gctr("prss->pr"))
        np.putmask(h1es["CD"], ix("IE"), h1es["CD"] - 1.0 * gctr("srqs->qr"))

        np.putmask(g2es["DCDC"], ix("IEII"), g2es["DCDC"] + 1.0 * gctr("prqs->sprq"))
        np.putmask(g2es["CCDD"], ix("EEII"), g2es["CCDD"] + 0.5 * gctr("prqs->pqsr"))
        np.putmask(g2es["CCDD"], ix("EEEI"), g2es["CCDD"] + 0.5 * gctr("prqs->pqsr"))
        np.putmask(g2es["DCDC"], ix("IEEI"), g2es["DCDC"] + 1.0 * gctr("prqs->sprq"))
        np.putmask(g2es["CCDD"], ix("EIEE"), g2es["CCDD"] + 1.0 * gctr("prqs->pqsr"))
        np.putmask(g2es["CCDD"], ix("EEIE"), g2es["CCDD"] + 0.5 * gctr("prqs->pqsr"))
        np.putmask(g2es["CCDD"], ix("EEEE"), g2es["CCDD"] + 0.5 * gctr("prqs->pqsr"))
        np.putmask(g2es["DDCC"], ix("IIII"), g2es["DDCC"] + 0.5 * gctr("prqs->rsqp"))
        np.putmask(g2es["DCDC"], ix("IIEI"), g2es["DCDC"] + 1.0 * gctr("prqs->sprq"))
        np.putmask(g2es["CCDD"], ix("IIEE"), g2es["CCDD"] + 0.5 * gctr("prqs->pqsr"))
        np.putmask(g2es["CCDD"], ix("EIEI"), g2es["CCDD"] + 1.0 * gctr("prqs->qprs"))

        h1es = {"(%s+%s)0" % tuple(k): np.sqrt(2) * v for k, v in h1es.items()}
        g2es = {"((%s+(%s+%s)0)1+%s)0" % tuple(k): 2 * v for k, v in g2es.items()}

        return h1es, g2es, const_es

    @staticmethod
    def make_sz(h1e, g2e, const_e, cidx):
        import numpy as np

        g2e = [g2e[0], g2e[1], g2e[1].transpose(2, 3, 0, 1), g2e[2]]
        ix = NormalOrder.def_ix(cidx)
        gctr = NormalOrder.def_gctr_sz(cidx, h1e, g2e)

        h1k = [("cd", "dc"), ("CD", "DC")]
        g2k = [
            "cddc:cdcd:ccdd:cdcd:ccdd:ddcc:dccd:cddc:cdcd:cdcd:dccd:cdcd:ccdd",
            "cdDC:cdCD:cCdD:cDCd:cCDd:dDcC:dcCD:CdDc:CdcD:CdcD:DcCd:CDcd:CcdD",
            "CDdc:CDcd:CcDd:CdcD:CcdD:DdCc:DCcd:cDdC:cDCd:cDCd:dCcD:cdCD:cCDd",
            "CDDC:CDCD:CCDD:CDCD:CCDD:DDCC:DCCD:CDDC:CDCD:CDCD:DCCD:CDCD:CCDD",
        ]
        g2k = [tuple(x.split(":")) for x in g2k]

        h1es = {k: np.zeros_like(h1e[i]) for i in range(2) for k in set(h1k[i])}
        g2es = {k: np.zeros_like(g2e[i]) for i in range(4) for k in set(g2k[i])}
        const_es = const_e

        const_es += 1.0 * (gctr(0, "qq->") + gctr(1, "qq->"))
        const_es += 0.5 * sum(gctr(i, "qqss->") for i in range(4))
        const_es -= 0.5 * (gctr(0, "sqqs->") + gctr(3, "sqqs->"))

        for i in range(2):
            cd, dc = h1k[i]
            np.putmask(h1es[cd], ix("EI"), h1es[cd] + gctr(i, "pq->pq"))
            np.putmask(h1es[cd], ix("EE"), h1es[cd] + gctr(i, "pq->pq"))
            np.putmask(h1es[dc], ix("II"), h1es[dc] - gctr(i, "pq->qp"))
            np.putmask(h1es[cd], ix("IE"), h1es[cd] + gctr(i, "pq->pq"))

        for i in range(2):
            cd, dc = h1k[i]
            np.putmask(h1es[cd], ix("EI"), h1es[cd] - 1.0 * gctr(i * 3, "pqqs->ps"))
            np.putmask(h1es[cd], ix("EI"), h1es[cd] + 1.0 * gctr(i * 3, "pqss->pq"))
            np.putmask(h1es[cd], ix("EE"), h1es[cd] + 1.0 * gctr(i * 3, "pqss->pq"))
            np.putmask(h1es[dc], ix("II"), h1es[dc] + 1.0 * gctr(i * 3, "pqqs->sp"))
            np.putmask(h1es[dc], ix("II"), h1es[dc] - 1.0 * gctr(i * 3, "pqss->qp"))
            np.putmask(h1es[cd], ix("IE"), h1es[cd] + 1.0 * gctr(i * 3, "pqss->pq"))
            np.putmask(h1es[cd], ix("IE"), h1es[cd] - 1.0 * gctr(i * 3, "sqrs->rq"))
            np.putmask(h1es[cd], ix("EE"), h1es[cd] - 1.0 * gctr(i * 3, "sqrs->rq"))
            np.putmask(h1es[cd], ix("EI"), h1es[cd] + 0.5 * gctr(i + 1, "pqss->pq"))
            np.putmask(h1es[cd], ix("EE"), h1es[cd] + 0.5 * gctr(i + 1, "pqss->pq"))
            np.putmask(h1es[dc], ix("II"), h1es[dc] - 0.5 * gctr(i + 1, "pqss->qp"))
            np.putmask(h1es[cd], ix("IE"), h1es[cd] + 0.5 * gctr(i + 1, "pqss->pq"))
            np.putmask(h1es[dc], ix("II"), h1es[dc] - 0.5 * gctr(2 - i, "rrqs->sq"))
            np.putmask(h1es[cd], ix("IE"), h1es[cd] + 0.5 * gctr(2 - i, "rrqs->qs"))
            np.putmask(h1es[cd], ix("EI"), h1es[cd] + 0.5 * gctr(2 - i, "rrqs->qs"))
            np.putmask(h1es[cd], ix("EE"), h1es[cd] + 0.5 * gctr(2 - i, "rrqs->qs"))

        for i in range(4):
            cdDC, cdCD, cCdD, cDCd, cCDd, dDcC, dcCD = g2k[i][:7]
            CdDc, CdcD, CdcD, DcCd, CDcd, CcdD = g2k[i][7:]
            np.putmask(g2es[cdDC], ix("EIII"), g2es[cdDC] - 0.5 * gctr(i, "prqs->prsq"))
            np.putmask(g2es[cCdD], ix("EEII"), g2es[cCdD] - 0.5 * gctr(i, "prqs->pqrs"))
            np.putmask(g2es[cCdD], ix("EEIE"), g2es[cCdD] - 0.5 * gctr(i, "prqs->pqrs"))
            np.putmask(g2es[cCdD], ix("EIEE"), g2es[cCdD] - 0.5 * gctr(i, "prqs->pqrs"))
            np.putmask(g2es[cCDd], ix("EEIE"), g2es[cCDd] + 0.5 * gctr(i, "prqs->pqsr"))
            np.putmask(g2es[cCdD], ix("EEEE"), g2es[cCdD] - 0.5 * gctr(i, "prqs->pqrs"))
            np.putmask(g2es[dDcC], ix("IIII"), g2es[dDcC] - 0.5 * gctr(i, "prqs->rspq"))
            np.putmask(g2es[dcCD], ix("IIIE"), g2es[dcCD] - 0.5 * gctr(i, "prqs->rpqs"))
            np.putmask(g2es[CdDc], ix("EIII"), g2es[CdDc] + 0.5 * gctr(i, "prqs->qrsp"))
            np.putmask(g2es[DcCd], ix("IIIE"), g2es[DcCd] + 0.5 * gctr(i, "prqs->spqr"))
            np.putmask(g2es[cCdD], ix("IIEE"), g2es[cCdD] - 0.5 * gctr(i, "prqs->pqrs"))
            np.putmask(g2es[CcdD], ix("EIEE"), g2es[CcdD] + 0.5 * gctr(i, "prqs->qprs"))

            eiie = ix("EIIE")
            if i == 0 or i == 3:
                np.putmask(g2es[cDCd], eiie, g2es[cDCd] - 1.0 * gctr(i, "prqs->psqr"))
                np.putmask(g2es[CDcd], eiie, g2es[CDcd] + 1.0 * gctr(i, "prqs->qspr"))
            else:
                np.putmask(g2es[cDCd], eiie, g2es[cDCd] - 0.5 * gctr(i, "prqs->psqr"))
                np.putmask(g2es[CdcD], eiie, g2es[CdcD] - 0.5 * gctr(i, "prqs->qrps"))
                np.putmask(g2es[CDcd], eiie, g2es[CDcd] + 0.5 * gctr(i, "prqs->qspr"))
                np.putmask(g2es[cdCD], eiie, g2es[cdCD] + 0.5 * gctr(i, "prqs->prqs"))

        return h1es, g2es, const_es

    @staticmethod
    def make_sgf(h1e, g2e, const_e, cidx):
        import numpy as np

        ix = NormalOrder.def_ix(cidx)
        gctr = NormalOrder.def_gctr(cidx, h1e, g2e)

        h1es = {k: np.zeros_like(h1e) for k in ("CD", "DC")}
        g2es = {k: np.zeros_like(g2e) for k in ("CDDC", "CCDD", "CDCD", "DDCC", "DCCD")}
        const_es = const_e

        const_es += 1.0 * gctr("qq->")
        const_es += 0.5 * gctr("qqss->")
        const_es -= 0.5 * gctr("sqqs->")

        np.putmask(h1es["CD"], ix("EI"), h1es["CD"] + gctr("pq->pq"))
        np.putmask(h1es["CD"], ix("EE"), h1es["CD"] + gctr("pq->pq"))
        np.putmask(h1es["DC"], ix("II"), h1es["DC"] - gctr("pq->qp"))
        np.putmask(h1es["CD"], ix("IE"), h1es["CD"] + gctr("pq->pq"))

        np.putmask(h1es["CD"], ix("EI"), h1es["CD"] - 1.0 * gctr("pqqs->ps"))
        np.putmask(h1es["CD"], ix("EI"), h1es["CD"] + 1.0 * gctr("pqss->pq"))
        np.putmask(h1es["CD"], ix("EE"), h1es["CD"] + 1.0 * gctr("pqss->pq"))
        np.putmask(h1es["DC"], ix("II"), h1es["DC"] + 1.0 * gctr("pqqs->sp"))
        np.putmask(h1es["DC"], ix("II"), h1es["DC"] - 1.0 * gctr("pqss->qp"))
        np.putmask(h1es["CD"], ix("IE"), h1es["CD"] + 1.0 * gctr("pqss->pq"))
        np.putmask(h1es["CD"], ix("IE"), h1es["CD"] - 1.0 * gctr("sqrs->rq"))
        np.putmask(h1es["CD"], ix("EE"), h1es["CD"] - 1.0 * gctr("sqrs->rq"))

        np.putmask(g2es["CDDC"], ix("EIII"), g2es["CDDC"] - 0.5 * gctr("pqrs->pqsr"))
        np.putmask(g2es["CCDD"], ix("EEII"), g2es["CCDD"] - 0.5 * gctr("pqrs->prqs"))
        np.putmask(g2es["CCDD"], ix("EEIE"), g2es["CCDD"] - 0.5 * gctr("pqrs->prqs"))
        np.putmask(g2es["CDCD"], ix("EIIE"), g2es["CDCD"] - 1.0 * gctr("pqrs->psrq"))
        np.putmask(g2es["CCDD"], ix("EIEE"), g2es["CCDD"] - 0.5 * gctr("pqrs->prqs"))
        np.putmask(g2es["CCDD"], ix("EEIE"), g2es["CCDD"] + 0.5 * gctr("pqrs->prsq"))
        np.putmask(g2es["CCDD"], ix("EEEE"), g2es["CCDD"] - 0.5 * gctr("pqrs->prqs"))
        np.putmask(g2es["DDCC"], ix("IIII"), g2es["DDCC"] - 0.5 * gctr("pqrs->qspr"))
        np.putmask(g2es["DCCD"], ix("IIIE"), g2es["DCCD"] - 0.5 * gctr("pqrs->qprs"))
        np.putmask(g2es["CDDC"], ix("EIII"), g2es["CDDC"] + 0.5 * gctr("pqrs->rqsp"))
        np.putmask(g2es["DCCD"], ix("IIIE"), g2es["DCCD"] + 0.5 * gctr("pqrs->sprq"))
        np.putmask(g2es["CCDD"], ix("IIEE"), g2es["CCDD"] - 0.5 * gctr("pqrs->prqs"))
        np.putmask(g2es["CDCD"], ix("EIIE"), g2es["CDCD"] + 1.0 * gctr("pqrs->rspq"))
        np.putmask(g2es["CCDD"], ix("EIEE"), g2es["CCDD"] + 0.5 * gctr("pqrs->rpqs"))

        return h1es, g2es, const_es


class ExprBuilder:
    def __init__(self, bw=Block2Wrapper()):
        self.data = bw.bx.GeneralFCIDUMP()
        if SymmetryTypes.SU2 in bw.symm_type:
            self.data.elem_type = bw.b.ElemOpTypes.SU2
        elif SymmetryTypes.SZ in bw.symm_type:
            self.data.elem_type = bw.b.ElemOpTypes.SZ
        elif SymmetryTypes.SGF in bw.symm_type:
            self.data.elem_type = bw.b.ElemOpTypes.SGF
        elif SymmetryTypes.SGB in bw.symm_type:
            self.data.elem_type = bw.b.ElemOpTypes.SGB
        self.data.const_e = 0.0
        self.bw = bw

    def add_const(self, x):
        self.data.const_e = self.data.const_e + x
        return self

    def add_term(self, expr, idx, val):
        self.data.exprs.append(expr)
        self.data.indices.append(self.bw.b.VectorUInt16(idx))
        self.data.data.append(self.bw.VectorFL([val]))
        return self

    def add_sum_term(self, expr, arr, cutoff=1e-12):
        import numpy as np

        self.data.exprs.append(expr)
        idx, dt = [], []
        for ix in np.ndindex(*arr.shape):
            if abs(arr[ix]) > cutoff:
                idx.extend(ix)
                dt.append(arr[ix])
        self.data.indices.append(self.bw.b.VectorUInt16(idx))
        self.data.data.append(self.bw.VectorFL(dt))
        return self

    def add_terms(self, expr, arr, idx, cutoff=1e-12):
        self.data.exprs.append(expr)
        didx, dt = [], []
        for ix, v in zip(idx, arr):
            if abs(v) > cutoff:
                didx.extend(ix)
                dt.append(v)
        self.data.indices.append(self.bw.b.VectorUInt16(didx))
        self.data.data.append(self.bw.VectorFL(dt))
        return self

    def iscale(self, d):
        import numpy as np

        for i, ix in enumerate(self.data.data):
            self.data.data[i] = self.bw.VectorFL(d * np.array(ix))
        return self

    def finalize(self):
        self.data = self.data.adjust_order()
        return self.data
