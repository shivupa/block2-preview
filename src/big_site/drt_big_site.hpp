
/*
 * block2: Efficient MPO implementation of quantum chemistry DMRG
 * Copyright (C) 2020-2023 Huanchen Zhai <hczhai@caltech.edu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include "../core/allocator.hpp"
#include "../core/cg.hpp"
#include "../core/state_info.hpp"
#include "../core/threading.hpp"
#include "../dmrg/general_mpo.hpp"
#include "big_site.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <unordered_map>
#include <vector>

using namespace std;

namespace block2 {

// Distinct Row Table
template <typename S, ElemOpTypes T> struct DRT {
    typedef long long LL;
    vector<array<int16_t, 3>> abc;
    vector<typename S::pg_t> pgs;
    vector<typename S::pg_t> orb_sym;
    vector<array<int, 4>> jds;
    vector<array<LL, 5>> xs;
    int n_sites, n_init_qs;
    DRT() : n_sites(0), n_init_qs(0) {}
    DRT(int16_t a, int16_t b, int16_t c,
        typename S::pg_t ipg = (typename S::pg_t)0,
        const vector<typename S::pg_t> &orb_sym = vector<typename S::pg_t>())
        : DRT(a + abs(b) + c, vector<S>{S(a + a + b, b, ipg)}, orb_sym) {}
    DRT(int n_sites, S q,
        const vector<typename S::pg_t> &orb_sym = vector<typename S::pg_t>())
        : DRT(n_sites, vector<S>{q}, orb_sym) {}
    DRT(int n_sites, const vector<S> &init_qs,
        const vector<typename S::pg_t> &orb_sym = vector<typename S::pg_t>())
        : n_sites(n_sites), orb_sym(orb_sym), n_init_qs((int)init_qs.size()) {
        if (T == ElemOpTypes::SU2 || T == ElemOpTypes::SZ) {
            for (auto &q : init_qs) {
                abc.push_back(array<int16_t, 3>{
                    (int16_t)((q.n() - q.twos()) >> 1), (int16_t)q.twos(),
                    (int16_t)(n_sites - ((q.n() + q.twos()) >> 1))});
                pgs.push_back(q.pg());
            }
        } else
            assert(false);
        if (this->orb_sym.size() == 0)
            this->orb_sym.resize(n_sites, (typename S::pg_t)0);
        initialize();
    }
    virtual ~DRT() = default;
    int n_rows() const { return (int)abc.size(); }
    void initialize() {
        abc.resize(n_init_qs);
        pgs.resize(n_init_qs);
        auto make_abc = [](int16_t a, int16_t b, int16_t c,
                           int16_t d) -> array<int16_t, 3> {
            switch (d) {
            case 0:
                return array<int16_t, 3>{a, b, (int16_t)(c - 1)};
            case 1:
                return array<int16_t, 3>{(int16_t)(a - (b <= 0)),
                                         (int16_t)(b - 1),
                                         (int16_t)(c - (b <= 0))};
            case 2:
                return array<int16_t, 3>{(int16_t)(a - (b >= 0)),
                                         (int16_t)(b + 1),
                                         (int16_t)(c - (b >= 0))};
            case 3:
                return array<int16_t, 3>{(int16_t)(a - 1), b, c};
            default:
                return array<int16_t, 3>{-1, -1, -1};
            }
        };
        auto allow_abc = [](int16_t a, int16_t b, int16_t c,
                            int16_t d) -> bool {
            switch (d) {
            case 0:
                return c;
            case 1:
                return T == ElemOpTypes::SU2 ? b : (b > 0 || a * c);
            case 2:
                return T == ElemOpTypes::SU2 ? a * c : (b < 0 || a * c);
            case 3:
                return a;
            default:
                return false;
            }
        };
        auto make_pg = [](typename S::pg_t g, typename S::pg_t gk, int16_t d) ->
            typename S::pg_t {
                return (d & 1) ^ (d >> 1) ? S::pg_mul(gk, g) : g;
            };
        auto allow_pg = [](int k, typename S::pg_t g, typename S::pg_t gk,
                           int16_t d) -> bool {
            return k != 0 || ((d & 1) ^ (d >> 1) ? S::pg_mul(gk, g) : g) == 0;
        };
        auto compare_abc_pg =
            [](const pair<array<int16_t, 3>, typename S::pg_t> &p,
               const pair<array<int16_t, 3>, typename S::pg_t> &q) {
                return p.first != q.first ? p.first > q.first
                                          : p.second > q.second;
            };
        vector<vector<pair<array<int16_t, 3>, typename S::pg_t>>> pabc(n_sites +
                                                                       1);
        for (size_t i = 0; i < abc.size(); i++)
            pabc[0].push_back(make_pair(abc[i], pgs[i]));
        // construct graph
        for (int k = n_sites - 1, j = 0; k >= 0; k--, j++) {
            vector<pair<array<int16_t, 3>, typename S::pg_t>> &kabc =
                pabc[j + 1];
            for (const auto &abcg : pabc[j]) {
                const array<int16_t, 3> &x = abcg.first;
                const typename S::pg_t &g = abcg.second;
                for (int16_t d = 0; d < 4; d++)
                    if (allow_abc(x[0], x[1], x[2], d) &&
                        allow_pg(k, g, orb_sym[k], d))
                        kabc.push_back(make_pair(make_abc(x[0], x[1], x[2], d),
                                                 make_pg(g, orb_sym[k], d)));
            }
            sort(kabc.begin(), kabc.end(), compare_abc_pg);
            kabc.resize(
                distance(kabc.begin(), unique(kabc.begin(), kabc.end())));
        }
        int n_abc = 1;
        // filter graph
        for (int k = n_sites - 1, j, i; k >= 0; k--, n_abc += j) {
            vector<pair<array<int16_t, 3>, typename S::pg_t>> &kabc = pabc[k];
            const vector<pair<array<int16_t, 3>, typename S::pg_t>> &fabc =
                pabc[k + 1];
            for (i = 0, j = 0; i < kabc.size(); i++) {
                const array<int16_t, 3> &x = kabc[i].first;
                const typename S::pg_t &g = kabc[i].second;
                bool found = false;
                for (int16_t d = 0; d < 4 && !found; d++)
                    found =
                        found ||
                        binary_search(
                            fabc.begin(), fabc.end(),
                            make_pair(make_abc(x[0], x[1], x[2], d),
                                      make_pg(g, orb_sym[n_sites - 1 - k], d)),
                            compare_abc_pg);
                if (found)
                    kabc[j++] = kabc[i];
            }
            kabc.resize(j);
        }
        // construct abc
        abc.clear(), pgs.clear();
        abc.reserve(n_abc), pgs.reserve(n_abc);
        for (auto &kabc : pabc)
            for (auto &abcg : kabc)
                abc.push_back(abcg.first), pgs.push_back(abcg.second);
        // construct jds
        jds.clear();
        jds.reserve(n_abc);
        for (int k = n_sites - 1, j = 0, p = 0; k >= 0; k--, j++) {
            p += pabc[j].size();
            for (auto &abcg : pabc[j]) {
                array<int, 4> jd;
                for (int16_t d = 0; d < 4; d++) {
                    auto v = make_pair(make_abc(abcg.first[0], abcg.first[1],
                                                abcg.first[2], d),
                                       make_pg(abcg.second, orb_sym[k], d));
                    auto it = lower_bound(pabc[j + 1].begin(),
                                          pabc[j + 1].end(), v, compare_abc_pg);
                    jd[d] = it != pabc[j + 1].end() && *it == v
                                ? p + (int)(it - pabc[j + 1].begin())
                                : 0;
                }
                jds.push_back(jd);
            }
        }
        jds.push_back(array<int, 4>{0, 0, 0, 0});
        // construct xs
        xs.clear();
        xs.resize(max(1, n_abc), array<LL, 5>{0, 0, 0, 0, 0});
        xs.back() = array<LL, 5>{0, 0, 0, 0, 1};
        for (int j = n_abc - 2; j >= 0; j--)
            for (int16_t d = 0; d < 4; d++)
                xs[j][d + 1] = xs[j][d] + xs[jds[j][d]][4] * (jds[j][d] != 0);
    }
    string operator[](LL i) const {
        string r(n_sites, ' ');
        int j = 0;
        for (; j < n_init_qs && i >= xs[j].back(); j++)
            i -= xs[j].back();
        for (int k = n_sites - 1; k >= 0; k--) {
            uint8_t d = (uint8_t)(upper_bound(xs[j].begin(), xs[j].end(), i) -
                                  1 - xs[j].begin());
            i -= xs[j][d], j = jds[j][d], r[k] = "0+-2"[d];
        }
        return r;
    }
    LL index(const string &x) const {
        LL i = 0;
        int j = 0;
        if (n_init_qs > 1) {
            array<int16_t, 3> iabc = array<int16_t, 3>{0, 0, 0};
            typename S::pg_t ipg = (typename S::pg_t)0;
            for (int k = 0; k < n_sites; k++)
                if (x[k] == '0')
                    iabc = array<int16_t, 3>{iabc[0], iabc[1],
                                             (int16_t)(iabc[2] + 1)};
                else if (x[k] == '+')
                    iabc = array<int16_t, 3>{iabc[0], (int16_t)(iabc[1] + 1),
                                             iabc[2]},
                    ipg = S::pg_mul(ipg, orb_sym[k]);
                else if (x[k] == '-')
                    iabc = array<int16_t, 3>{(int16_t)(iabc[0] + 1),
                                             (int16_t)(iabc[1] - 1),
                                             (int16_t)(iabc[2] + 1)},
                    ipg = S::pg_mul(ipg, orb_sym[k]);
                else
                    iabc = array<int16_t, 3>{(int16_t)(iabc[0] + 1), iabc[1],
                                             iabc[2]};
            for (; j < n_init_qs && (iabc != abc[j] || ipg != pgs[j]); j++)
                i += xs[j].back();
        }
        for (int j = 0, k = n_sites - 1; k >= 0; k--) {
            uint8_t d = (uint8_t)string("0+-2").find(x[k]);
            i += xs[j][d], j = jds[j][d];
        }
        return i;
    }
    LL size() const { return xs[0].back(); }
    int q_index(S q) const {
        for (int j = 0; j < n_init_qs; j++)
            if (S(abc[j][0] + abc[j][0] + abc[j][1], abc[j][1], pgs[j]) == q)
                return j;
        return -1;
    }
    pair<LL, LL> q_range(int i) const {
        LL a = 0, b = 0;
        for (int j = 0; j <= i; j++)
            a = b, b += xs[j].back();
        return make_pair(a, b);
    }
    shared_ptr<StateInfo<S>> get_basis() const {
        shared_ptr<StateInfo<S>> b = make_shared<StateInfo<S>>();
        b->allocate(n_init_qs);
        for (int i = 0; i < n_init_qs; i++) {
            b->quanta[i] =
                S(abc[i][0] + abc[i][0] + abc[i][1], abc[i][1], pgs[i]);
            b->n_states[i] = xs[i][4];
        }
        b->sort_states();
        return b;
    }
    string to_str() const {
        stringstream ss;
        ss << setw(4) << "J" << setw(6) << "K" << setw(4) << "A" << setw(4)
           << "B" << setw(4) << "C" << setw(6) << "PG" << setw(6) << "JD0"
           << setw(6) << "JD1" << setw(6) << "JD2" << setw(6) << "JD3"
           << " " << setw(12) << "X0"
           << " " << setw(12) << "X1"
           << " " << setw(12) << "X2"
           << " " << setw(12) << "X3" << endl;
        int n = n_rows();
        int pk = -1;
        for (int i = 0, k; i < n; i++, pk = k) {
            k = abc[i][0] + abc[i][1] + abc[i][2];
            ss << setw(4) << (i + 1);
            if (k == pk)
                ss << setw(6) << "";
            else
                ss << setw(6) << k;
            ss << setw(4) << abc[i][0] << setw(4) << abc[i][1] << setw(4)
               << abc[i][2];
            ss << setw(6) << (int)pgs[i];
            for (int16_t dk = 0; dk < 4; dk++)
                if (jds[i][dk] == 0)
                    ss << setw(6) << "";
                else
                    ss << setw(6) << jds[i][dk] + 1;
            for (int16_t dk = 0; dk < 4; dk++)
                ss << " " << setw(12) << xs[i][dk + 1];
            ss << endl;
        }
        return ss.str();
    }
};

// Hamiltonian Distinct Row Table
template <typename S, ElemOpTypes T> struct HDRT {
    typedef long long LL;
    vector<array<int16_t, 5>> qs;
    vector<typename S::pg_t> pgs;
    vector<typename S::pg_t> orb_sym;
    vector<int> jds;
    vector<LL> xs;
    int n_sites, n_init_qs, nd;
    map<pair<string, int8_t>, int> d_map;
    vector<array<int16_t, 6>> d_step;
    vector<pair<string, int8_t>> d_expr;
    HDRT() : n_sites(0), n_init_qs(0), nd(0) {}
    HDRT(int n_sites, const vector<pair<S, pair<int16_t, int16_t>>> &init_qs,
         const vector<typename S::pg_t> &orb_sym = vector<typename S::pg_t>())
        : n_sites(n_sites), n_init_qs((int)init_qs.size()), orb_sym(orb_sym),
          nd(0) {
        for (auto &q : init_qs) {
            qs.push_back(array<int16_t, 5>{
                (int16_t)n_sites, (int16_t)q.first.n(), (int16_t)q.first.twos(),
                q.second.first, q.second.second});
            pgs.push_back(q.first.pg());
        }
        if (this->orb_sym.size() == 0)
            this->orb_sym.resize(n_sites, (typename S::pg_t)0);
    }
    int n_rows() const { return (int)qs.size(); }
    void initialize_steps(const vector<shared_ptr<SpinPermScheme>> &schemes) {
        d_map.clear(), d_step.clear();
        d_map[make_pair("", 0)] = 0;
        // dk dn d2s dw dl dpg
        d_step.push_back(array<int16_t, 6>{1, 0, 0, 0, 0, 0});
        for (const auto &scheme : schemes) {
            for (int i = 0; i < (int)scheme->data.size(); i++) {
                set<string> exprs;
                for (const auto &m : scheme->data[i])
                    for (const auto &p : m.second)
                        exprs.insert(p.second);
                const vector<uint16_t> &pat = scheme->index_patterns[i];
                for (int k = 0, n = (int)pat.size(), l; k < n; k = l) {
                    for (l = k; l < n && pat[k] == pat[l];)
                        l++;
                    for (const auto &expr : exprs) {
                        string x = SpinPermRecoupling::get_sub_expr(expr, k, l);
                        int8_t dq =
                            SpinPermRecoupling::get_target_twos(
                                SpinPermRecoupling::get_sub_expr(expr, 0, l)) -
                            SpinPermRecoupling::get_target_twos(
                                SpinPermRecoupling::get_sub_expr(expr, 0, k));
                        if (!d_map.count(make_pair(x, dq))) {
                            int16_t xc =
                                (int16_t)count(x.begin(), x.end(), 'C');
                            int16_t xd =
                                (int16_t)count(x.begin(), x.end(), 'D');
                            d_map[make_pair(x, dq)] = (int)d_step.size();
                            d_step.push_back(array<int16_t, 6>{
                                1, (int16_t)(xc - xd), dq, 1,
                                (int16_t)(xc + xd),
                                (int16_t)(xc == xd
                                              ? 0
                                              : (xc > xd ? ((xc - xd) & 1)
                                                         : -((xd - xc) & 1)))});
                        }
                    }
                }
            }
        }
        nd = (int)d_map.size();
        d_expr.resize(nd);
        for (auto &dm : d_map)
            d_expr[dm.second] = dm.first;
    }
    void initialize() {
        qs.resize(n_init_qs);
        pgs.resize(n_init_qs);
        auto make_q = [](const array<int16_t, 5> &q,
                         const array<int16_t, 6> &d) -> array<int16_t, 5> {
            return array<int16_t, 5>{
                (int16_t)(q[0] - d[0]), (int16_t)(q[1] - d[1]),
                (int16_t)(q[2] - d[2]), (int16_t)(q[3] - d[3]),
                (int16_t)(q[4] - d[4])};
        };
        auto allow_q = [](const array<int16_t, 5> &q) -> bool {
            return (q[0] > 0 && (T != ElemOpTypes::SU2 || q[2] >= 0) &&
                    q[3] >= 0 && q[4] >= 0) ||
                   (q[0] == 0 && q[1] == 0 && q[2] == 0 && q[3] == 0 &&
                    q[4] == 0);
        };
        auto make_pg = [](typename S::pg_t g, typename S::pg_t gk,
                          const array<int16_t, 6> &d) ->
            typename S::pg_t { return d[5] != 0 ? S::pg_mul(gk, g) : g; };
        auto allow_pg = [](int k, typename S::pg_t g) -> bool {
            return k != 0 || g == 0;
        };
        auto compare_q_pg =
            [](const pair<array<int16_t, 5>, typename S::pg_t> &p,
               const pair<array<int16_t, 5>, typename S::pg_t> &q) {
                return p.first != q.first ? p.first > q.first
                                          : p.second > q.second;
            };
        vector<int16_t> ddq(nd);
        for (int16_t d = 0; d < nd; d++)
            ddq[d] = SpinPermRecoupling::get_target_twos(d_expr[d].first);
        vector<vector<pair<array<int16_t, 5>, typename S::pg_t>>> pqs(n_sites +
                                                                      1);
        for (size_t i = 0; i < qs.size(); i++)
            pqs[0].push_back(make_pair(qs[i], pgs[i]));
        // construct graph
        for (int k = n_sites - 1, j = 0; k >= 0; k--, j++) {
            vector<pair<array<int16_t, 5>, typename S::pg_t>> &kq = pqs[j + 1];
            for (const auto &qg : pqs[j]) {
                for (int16_t d = 0; d < nd; d++) {
                    const auto &nq = make_q(qg.first, d_step[d]);
                    const auto &ng = make_pg(qg.second, orb_sym[k], d_step[d]);
                    if (allow_q(nq) && allow_pg(k, ng) &&
                        (T != ElemOpTypes::SU2 ||
                         SU2CG::triangle(ddq[d], qg.first[2], nq[2])))
                        kq.push_back(make_pair(nq, ng));
                }
            }
            sort(kq.begin(), kq.end(), compare_q_pg);
            kq.resize(distance(kq.begin(), unique(kq.begin(), kq.end())));
        }
        int n_qs = 1;
        // filter graph
        for (int k = n_sites - 1, j, i; k >= 0; k--, n_qs += j) {
            vector<pair<array<int16_t, 5>, typename S::pg_t>> &kq = pqs[k];
            const vector<pair<array<int16_t, 5>, typename S::pg_t>> &fq =
                pqs[k + 1];
            for (i = 0, j = 0; i < kq.size(); i++) {
                bool found = false;
                for (int16_t d = 0; d < nd && !found; d++) {
                    const auto &nq = make_q(kq[i].first, d_step[d]);
                    const auto &ng = make_pg(
                        kq[i].second, orb_sym[n_sites - 1 - k], d_step[d]);
                    found =
                        found || binary_search(fq.begin(), fq.end(),
                                               make_pair(nq, ng), compare_q_pg);
                }
                if (found || k == 0)
                    kq[j++] = kq[i];
            }
            kq.resize(j);
        }
        // construct qs
        qs.clear(), pgs.clear();
        qs.reserve(n_qs), pgs.reserve(n_qs);
        for (auto &kq : pqs)
            for (auto &qg : kq)
                qs.push_back(qg.first), pgs.push_back(qg.second);
        // construct jds
        jds.clear();
        jds.reserve(n_qs * nd);
        for (int k = n_sites - 1, j = 0, p = 0; k >= 0; k--, j++) {
            p += pqs[j].size();
            for (auto &qg : pqs[j]) {
                for (int16_t d = 0; d < nd; d++) {
                    const auto &nqg =
                        make_pair(make_q(qg.first, d_step[d]),
                                  make_pg(qg.second, orb_sym[k], d_step[d]));
                    bool allowed =
                        allow_q(nqg.first) && allow_pg(k, nqg.second) &&
                        (T != ElemOpTypes::SU2 ||
                         SU2CG::triangle(ddq[d], qg.first[2], nqg.first[2]));
                    auto it = lower_bound(pqs[j + 1].begin(), pqs[j + 1].end(),
                                          nqg, compare_q_pg);
                    int jd = allowed && it != pqs[j + 1].end() && *it == nqg
                                 ? p + (int)(it - pqs[j + 1].begin())
                                 : 0;
                    jds.push_back(jd);
                }
            }
        }
        for (int16_t d = 0; d < nd; d++)
            jds.push_back(0);
        // construct xs
        xs.clear();
        xs.resize(max(1, n_qs * (nd + 1)), 0);
        for (int16_t d = 0; d < nd; d++)
            xs[(n_qs - 1) * (nd + 1) + d] = 0;
        xs[(n_qs - 1) * (nd + 1) + nd] = 1;
        for (int j = n_qs - 2; j >= 0; j--)
            for (int16_t d = 0; d < nd; d++)
                xs[j * (nd + 1) + d + 1] =
                    xs[j * (nd + 1) + d] + xs[jds[j * nd + d] * (nd + 1) + nd] *
                                               (jds[j * nd + d] != 0);
    }
    pair<string, vector<uint16_t>> operator[](LL i) const {
        string r = "";
        int rq = 0;
        vector<uint16_t> kidx;
        int j = 0;
        for (; i >= xs[j * (nd + 1) + nd]; j++)
            i -= xs[j * (nd + 1) + nd];
        for (int k = n_sites - 1; k >= 0; k--) {
            int16_t d =
                (int16_t)(upper_bound(xs.begin() + j * (nd + 1),
                                      xs.begin() + (j + 1) * (nd + 1), i) -
                          1 - (xs.begin() + j * (nd + 1)));
            i -= xs[j * (nd + 1) + d], j = jds[j * nd + d];
            pair<string, int8_t> dx = d_expr[d];
            if (dx.first != "") {
                for (size_t l = 0; l < d_step[d][4]; l++)
                    kidx.insert(kidx.begin(), (uint16_t)k);
                if (r == "")
                    r = dx.first, rq = d_step[d][2];
                else {
                    rq += d_step[d][2];
                    stringstream ss;
                    ss << "(" << dx.first << "+" << r << ")" << rq;
                    r = ss.str();
                }
            }
        }
        return make_pair(r, kidx);
    }
    LL index(const string &expr, const vector<uint16_t> &idxs) const {
        vector<int16_t> ds(n_sites, d_map.at(make_pair("", 0)));
        for (int k = 0, n = (int)idxs.size(), l; k < n; k = l) {
            for (l = k; l < n && idxs[k] == idxs[l];)
                l++;
            string x = SpinPermRecoupling::get_sub_expr(expr, k, l);
            int8_t dq = SpinPermRecoupling::get_target_twos(
                            SpinPermRecoupling::get_sub_expr(expr, 0, l)) -
                        SpinPermRecoupling::get_target_twos(
                            SpinPermRecoupling::get_sub_expr(expr, 0, k));
            if (!d_map.count(make_pair(x, dq)))
                throw runtime_error("expr not found : " + x + " dq = " +
                                    string(1, '0' + dq) + " expr = " + expr);
            ds[idxs[k]] = d_map.at(make_pair(x, dq));
        }
        array<int16_t, 5> iq = qs.back();
        typename S::pg_t ipg = pgs.back();
        for (int k = 0; k < n_sites; k++) {
            iq = array<int16_t, 5>{(int16_t)(iq[0] + d_step[ds[k]][0]),
                                   (int16_t)(iq[1] + d_step[ds[k]][1]),
                                   (int16_t)(iq[2] + d_step[ds[k]][2]),
                                   (int16_t)(iq[3] + d_step[ds[k]][3]),
                                   (int16_t)(iq[4] + d_step[ds[k]][4])};
            ipg = d_step[ds[k]][5] != 0 ? S::pg_mul(ipg, orb_sym[k]) : ipg;
        }
        LL i = 0;
        int j = 0;
        for (; j < n_init_qs && (iq != qs[j] || ipg != pgs[j]); j++)
            i += xs[j * (nd + 1) + nd];
        assert(j < n_init_qs);
        for (int k = n_sites - 1; k >= 0; k--)
            i += xs[j * (nd + 1) + ds[k]], j = jds[j * nd + ds[k]];
        return i;
    }
    LL size() const {
        LL r = 0;
        for (int i = 0; i < n_init_qs; i++)
            r += xs[i * (nd + 1) + nd];
        return r;
    }
    template <typename FL>
    shared_ptr<vector<FL>> fill_data(const vector<string> &exprs,
                                     const vector<vector<uint16_t>> &indices,
                                     const vector<vector<FL>> &data) const {
        shared_ptr<vector<FL>> r = make_shared<vector<FL>>(size(), (FL)0.0);
        for (size_t ix = 0; ix < exprs.size(); ix++) {
            const string &expr = exprs[ix];
            const int nn = SpinPermRecoupling::count_cds(expr);
            for (size_t j = 0; j < data[ix].size(); j++)
                (*r)[index(expr, vector<uint16_t>(indices[ix].begin() + j * nn,
                                                  indices[ix].begin() +
                                                      (j + 1) * nn))] +=
                    data[ix][j];
        }
        return r;
    }
    string to_str() const {
        stringstream ss;
        ss << setw(4) << "J" << setw(6) << "K" << setw(4) << "N" << setw(4)
           << "2S" << setw(4) << "W" << setw(4) << "L" << setw(6) << "PG";
        for (int16_t dk = 0; dk < nd; dk++)
            ss << setw(4 + (dk < 10)) << "JD" << (int)dk;
        for (int16_t dk = 0; dk < nd; dk++)
            ss << setw(4 + (dk < 10)) << "X" << (int)dk;
        ss << endl;
        int n = n_rows();
        int pk = -1;
        for (int i = 0, k; i < n; i++, pk = k) {
            ss << setw(4) << (i + 1);
            if (qs[i][0] == pk)
                ss << setw(6) << "";
            else
                ss << setw(6) << qs[i][0];
            ss << setw(4) << qs[i][1] << setw(4) << qs[i][2] << setw(4)
               << qs[i][3] << setw(4) << qs[i][4];
            ss << setw(6) << (int)pgs[i];
            for (int16_t dk = 0; dk < nd; dk++)
                if (jds[i * nd + dk] == 0)
                    ss << setw(6) << "";
                else
                    ss << setw(6) << jds[i * nd + dk] + 1;
            for (int16_t dk = 0; dk < nd; dk++)
                ss << setw(6) << xs[i * (nd + 1) + dk + 1];
            ss << endl;
        }
        return ss.str();
    }
};

template <typename FL> struct SU2Matrix {
    vector<FL> data;
    vector<pair<int16_t, int16_t>> indices;
    int16_t dq;
    SU2Matrix(int16_t dq, const vector<FL> &data,
              const vector<pair<int16_t, int16_t>> &indices)
        : dq(dq), indices(indices), data(data) {}
    static SU2CG &cg() {
        static SU2CG _cg;
        return _cg;
    }
    static const vector<SU2Matrix<FL>> &op_matrices() {
        static vector<SU2Matrix<FL>> _mats = vector<SU2Matrix<FL>>{
            SU2Matrix<FL>(0, vector<FL>{(FL)1.0, (FL)1.0, (FL)1.0},
                          vector<pair<int16_t, int16_t>>{make_pair(0, 0),
                                                         make_pair(1, 1),
                                                         make_pair(2, 2)}),
            SU2Matrix<FL>(1, vector<FL>{(FL)1.0, (FL)(-sqrtl(2))},
                          vector<pair<int16_t, int16_t>>{make_pair(1, 0),
                                                         make_pair(2, 1)}),
            SU2Matrix<FL>(1, vector<FL>{(FL)sqrtl(2), (FL)1.0},
                          vector<pair<int16_t, int16_t>>{make_pair(0, 1),
                                                         make_pair(1, 2)})};
        return _mats;
    }
    static SU2Matrix<FL> multiply(const SU2Matrix<FL> &a,
                                  const SU2Matrix<FL> &b, int16_t dq) {
        map<pair<int16_t, int16_t>, FL> r;
        for (int i = 0; i < (int)a.data.size(); i++)
            for (int j = 0; j < (int)b.data.size(); j++)
                if (a.indices[i].second == b.indices[j].first)
                    r[make_pair(a.indices[i].first, b.indices[j].second)] +=
                        a.data[i] * b.data[j] *
                        (FL)cg().racah(b.indices[j].second & 1, b.dq,
                                       a.indices[i].first & 1, a.dq,
                                       a.indices[i].second & 1, dq) *
                        (FL)sqrtl((dq + 1) * ((a.indices[i].second & 1) + 1)) *
                        (FL)cg().phase(a.dq, b.dq, dq);
        vector<FL> data;
        vector<pair<int16_t, int16_t>> indices;
        for (auto &x : r)
            if (x.second != (FL)0.0)
                indices.push_back(x.first), data.push_back(x.second);
        return SU2Matrix<FL>(dq, data, indices);
    }
    static SU2Matrix<FL> build_matrix(const string &expr) {
        if (expr == "")
            return op_matrices()[0];
        else if (expr == "C")
            return op_matrices()[1];
        else if (expr == "D")
            return op_matrices()[2];
        int ix = 0, depth = 0;
        for (auto &c : expr) {
            if (c == '(')
                depth++;
            else if (c == ')')
                depth--;
            else if (c == '+' && depth == 1)
                break;
            ix++;
        }
        int dq = 0, iy = 0;
        for (int i = (int)expr.length() - 1, k = 1; i >= 0; i--, k *= 10)
            if (expr[i] >= '0' && expr[i] <= '9')
                dq += (expr[i] - '0') * k;
            else {
                iy = i;
                break;
            }
        SU2Matrix<FL> a = build_matrix(expr.substr(1, ix - 1));
        SU2Matrix<FL> b = build_matrix(expr.substr(ix + 1, iy - ix - 1));
        return multiply(a, b, dq);
    }
    SU2Matrix<FL> expand() const {
        vector<FL> rd;
        vector<pair<int16_t, int16_t>> ri;
        for (int i = 0; i < (int)data.size(); i++) {
            int16_t p = indices[i].first, q = indices[i].second;
            p += (p >> 1), q += (q >> 1);
            if (p == 1 && q == 1) {
                for (int k = 1; k <= 2; k++)
                    for (int l = 1; l <= 2; l++)
                        ri.push_back(make_pair(k, l)), rd.push_back(data[i]);
            } else if (p == 1) {
                for (int k = 1; k <= 2; k++)
                    ri.push_back(make_pair(k, q)), rd.push_back(data[i]);
            } else if (q == 1) {
                for (int k = 1; k <= 2; k++)
                    ri.push_back(make_pair(p, k)), rd.push_back(data[i]);
            } else
                ri.push_back(make_pair(p, q)), rd.push_back(data[i]);
        }
        return SU2Matrix<FL>(dq, rd, ri);
    }
};

template <typename, typename, typename = void> struct DRTBigSite;

template <typename S, typename FL>
struct DRTBigSite<S, FL, typename S::is_su2_t> : BigSite<S, FL> {
    typedef typename GMatrix<FL>::FP FP;
    typedef long long LL;
    using BigSite<S, FL>::n_orbs;
    using BigSite<S, FL>::basis;
    using BigSite<S, FL>::op_infos;
    shared_ptr<FCIDUMP<FL>> fcidump = nullptr;
    shared_ptr<GeneralFCIDUMP<FL>> gfd = nullptr;
    shared_ptr<DRT<S, ElemOpTypes::SU2>> drt;
    shared_ptr<vector<FL>> factors;
    array<size_t, 7> factor_strides;
    bool is_right;
    int iprint;
    int n_total_orbs;
    const static int max_n = 10, max_s = 10;
    FP cutoff = 1E-14;
    DRTBigSite(const vector<S> &qs, bool is_right, int n_orbs,
               const vector<typename S::pg_t> &orb_sym,
               const shared_ptr<FCIDUMP<FL>> &fcidump = nullptr, int iprint = 0)
        : BigSite<S, FL>(n_orbs), is_right(is_right), fcidump(fcidump),
          n_total_orbs(fcidump == nullptr ? 0 : fcidump->n_sites()),
          iprint(iprint) {
        vector<typename S::pg_t> big_orb_sym(n_orbs);
        if (!is_right)
            for (int i = 0; i < n_orbs; i++)
                big_orb_sym[i] = orb_sym[i];
        else
            for (int i = 0; i < n_orbs; i++)
                big_orb_sym[i] = orb_sym[n_orbs - 1 - i];
        drt = make_shared<DRT<S, ElemOpTypes::SU2>>(n_orbs, qs, big_orb_sym);
        basis = drt->get_basis();
        op_infos = get_site_op_infos(orb_sym);
        prepare_factors();
    }
    virtual ~DRTBigSite() = default;
    static vector<S>
    get_target_quanta(bool is_right, int n_orbs, int n_max_elec,
                      const vector<typename S::pg_t> &orb_sym) {
        S vacuum, target(S::invalid);
        vector<shared_ptr<StateInfo<S>>> site_basis(n_orbs);
        for (int m = 0; m < n_orbs; m++) {
            shared_ptr<StateInfo<S>> b = make_shared<StateInfo<S>>();
            b->allocate(3);
            b->quanta[0] = vacuum;
            b->quanta[1] = S(1, 1, orb_sym[m]);
            b->quanta[2] = S(2, 0, 0);
            b->n_states[0] = b->n_states[1] = b->n_states[2] = 1;
            b->sort_states();
            site_basis[m] = b;
        }
        shared_ptr<StateInfo<S>> x = make_shared<StateInfo<S>>(vacuum);
        if (!is_right) {
            for (int i = 0; i < n_orbs; i++)
                x = make_shared<StateInfo<S>>(
                    StateInfo<S>::tensor_product(*x, *site_basis[i], target));
            int max_n = 0;
            for (int q = 0; q < x->n; q++)
                if (x->quanta[q].n() > max_n)
                    max_n = x->quanta[q].n();
            for (int q = 0; q < x->n; q++)
                if (x->quanta[q].n() < max_n - n_max_elec ||
                    x->quanta[q].twos() > n_max_elec)
                    x->n_states[q] = 0;
        } else {
            for (int i = n_orbs - 1; i >= 0; i--)
                x = make_shared<StateInfo<S>>(
                    StateInfo<S>::tensor_product(*site_basis[i], *x, target));
            for (int q = 0; q < x->n; q++)
                if (x->quanta[q].n() > n_max_elec)
                    x->n_states[q] = 0;
        }
        x->collect();
        return vector<S>(&x->quanta[0], &x->quanta[0] + x->n);
    }
    static vector<shared_ptr<vector<FL>>>
    fill_integral_data(const shared_ptr<HDRT<S, ElemOpTypes::SU2>> &hdrt,
                       const vector<shared_ptr<SpinPermScheme>> &schemes,
                       const vector<shared_ptr<GeneralFCIDUMP<FL>>> &gfds) {
        map<string, map<vector<uint16_t>, int>> expr_mp;
        for (const auto &scheme : schemes)
            for (int i = 0; i < (int)scheme->data.size(); i++)
                for (const auto &d : scheme->data[i])
                    for (const auto &dex : d.second)
                        if (!expr_mp[dex.second].count(
                                scheme->index_patterns[i]))
                            expr_mp[dex.second][scheme->index_patterns[i]] = 0;
        int im = 0;
        for (auto &m : expr_mp)
            for (auto &mm : m.second)
                mm.second = im++;
        vector<vector<int16_t>> ds(im);
        vector<map<typename S::pg_t, pair<int, LL>>> jis(im);
        for (auto &m : expr_mp)
            for (auto &mm : m.second) {
                im = mm.second;
                for (int k = 0, n = (int)mm.first.size(), l; k < n; k = l) {
                    for (l = k; l < n && mm.first[k] == mm.first[l];)
                        l++;
                    string x = SpinPermRecoupling::get_sub_expr(m.first, k, l);
                    int8_t dq =
                        SpinPermRecoupling::get_target_twos(
                            SpinPermRecoupling::get_sub_expr(m.first, 0, l)) -
                        SpinPermRecoupling::get_target_twos(
                            SpinPermRecoupling::get_sub_expr(m.first, 0, k));
                    if (!hdrt->d_map.count(make_pair(x, dq)))
                        throw runtime_error("expr not found : " + x +
                                            " dq = " + string(1, '0' + dq) +
                                            " expr = " + m.first);
                    ds[im].push_back(hdrt->d_map.at(make_pair(x, dq)));
                }
                array<int16_t, 5> iq = hdrt->qs.back();
                for (int k = 0; k < (int)ds[im].size(); k++)
                    iq = array<int16_t, 5>{
                        (int16_t)(iq[0] + hdrt->d_step[ds[im][k]][0]),
                        (int16_t)(iq[1] + hdrt->d_step[ds[im][k]][1]),
                        (int16_t)(iq[2] + hdrt->d_step[ds[im][k]][2]),
                        (int16_t)(iq[3] + hdrt->d_step[ds[im][k]][3]),
                        (int16_t)(iq[4] + hdrt->d_step[ds[im][k]][4])};
                iq[0] = (int16_t)hdrt->n_sites;
                LL i = 0;
                for (int j = 0; j < hdrt->n_init_qs; j++) {
                    if (iq == hdrt->qs[j])
                        jis[im][hdrt->pgs[j]] = make_pair(j, i);
                    i += hdrt->xs[j * (hdrt->nd + 1) + hdrt->nd];
                }
            }
        vector<vector<pair<int, LL>>> hjumps(
            hdrt->n_rows(), vector<pair<int, LL>>{make_pair(0, 0)});
        for (int j = hdrt->n_rows() - 1, k; j >= 0; j--) {
            hjumps[j][0].first = j;
            if ((k = hdrt->jds[j * hdrt->nd + 0]) != 0) {
                hjumps[j].insert(hjumps[j].end(), hjumps[k].begin(),
                                 hjumps[k].end());
                const LL x = hdrt->xs[j * (hdrt->nd + 1) + 0];
                for (int l = 1; l < (int)hjumps[j].size(); l++)
                    hjumps[j][l].second += x;
            }
        }
        int ntg = threading->activate_global();
        vector<shared_ptr<vector<FL>>> r(gfds.size());
        for (size_t ig = 0; ig < gfds.size(); ig++) {
            r[ig] = make_shared<vector<FL>>(hdrt->size(), (FL)0.0);
            for (size_t ix = 0; ix < gfds[ig]->exprs.size(); ix++) {
                const string &expr = gfds[ig]->exprs[ix];
                const int nn = SpinPermRecoupling::count_cds(expr);
                const map<vector<uint16_t>, int> &xmp = expr_mp.at(expr);
#pragma omp parallel for schedule(static, 100) num_threads(ntg)
                for (size_t ip = 0; ip < gfds[ig]->indices[ix].size();
                     ip += nn) {
                    vector<uint16_t> idx(gfds[ig]->indices[ix].begin() + ip,
                                         gfds[ig]->indices[ix].begin() + ip +
                                             nn);
                    vector<uint16_t> idx_mat(nn);
                    if (nn >= 1)
                        idx_mat[0] = 0;
                    for (int j = 1; j < nn; j++)
                        idx_mat[j] = idx_mat[j - 1] + (idx[j] != idx[j - 1]);
                    typename S::pg_t ipg = hdrt->pgs.back();
                    for (auto &x : idx)
                        ipg = S::pg_mul(ipg, hdrt->orb_sym[x]);
                    if (!jis[im].count(ipg)) {
                        throw runtime_error("Small integral elements violating "
                                            "point group symmetry!");
                    }
                    int im = xmp.at(idx_mat), j = jis[im].at(ipg).first,
                        k = hdrt->n_sites - 1;
                    LL i = jis[im].at(ipg).second;
                    vector<int16_t> &xds = ds[im];
                    for (int l = nn - 1, g, m = (int)xds.size() - 1; l >= 0;
                         l = g, m--, k--) {
                        for (g = l; g >= 0 && idx[g] == idx[l];)
                            g--;
                        i += hjumps[j][k - idx[l]].second;
                        j = hjumps[j][k - idx[l]].first;
                        i += hdrt->xs[j * (hdrt->nd + 1) + xds[m]];
                        j = hdrt->jds[j * hdrt->nd + xds[m]];
                        k = idx[l];
                    }
                    i += hjumps[j][k + 1].second;
                    (*r[ig])[i] = gfds[ig]->data[ix][ip / nn];
                }
            }
        }
        threading->activate_normal();
        return r;
    }
    vector<pair<S, shared_ptr<SparseMatrixInfo<S>>>>
    get_site_op_infos(const vector<uint8_t> &orb_sym) {
        shared_ptr<VectorAllocator<uint32_t>> i_alloc =
            make_shared<VectorAllocator<uint32_t>>();
        map<S, shared_ptr<SparseMatrixInfo<S>>> info;
        const int max_n_odd = max_n | 1, max_s_odd = max_s | 1;
        const int max_n_even = max_n_odd ^ 1, max_s_even = max_s_odd ^ 1;
        info[S(0)] = nullptr;
        for (auto ipg : orb_sym) {
            for (int n = -max_n_odd; n <= max_n_odd; n += 2)
                for (int s = 1; s <= max_s_odd; s += 2) {
                    info[S(n, s, ipg)] = nullptr;
                    info[S(n, s, S::pg_inv(ipg))] = nullptr;
                }
            for (auto jpg : orb_sym)
                for (int n = -max_n_even; n <= max_n_even; n += 2)
                    for (int s = 0; s <= max_s_even; s += 2) {
                        info[S(n, s, S::pg_mul(ipg, jpg))] = nullptr;
                        info[S(n, s, S::pg_mul(ipg, S::pg_inv(jpg)))] = nullptr;
                        info[S(n, s, S::pg_mul(S::pg_inv(ipg), jpg))] = nullptr;
                        info[S(n, s,
                               S::pg_mul(S::pg_inv(ipg), S::pg_inv(jpg)))] =
                            nullptr;
                    }
        }
        for (auto &p : info) {
            p.second = make_shared<SparseMatrixInfo<S>>(i_alloc);
            p.second->initialize(*basis, *basis, p.first, p.first.is_fermion());
        }
        return vector<pair<S, shared_ptr<SparseMatrixInfo<S>>>>(info.begin(),
                                                                info.end());
    }
    void prepare_factors() {
        int16_t max_bb = 0, max_bk = 0, max_bh = max_s, max_dh = max_s;
        for (auto &p : drt->abc)
            max_bb = max(max_bb, p[1]);
        for (auto &p : drt->abc)
            max_bk = max(max_bk, p[1]);
        array<int, 7> factor_shape = array<int, 7>{
            max_bb + 1, 3, max_bk + 1, 3, max_bh + 1, max_bh + 1, max_dh + 1};
        factor_strides[6] = 1;
        for (int i = 6; i > 0; i--)
            factor_strides[i - 1] = factor_strides[i] * factor_shape[i];
        factors = make_shared<vector<FL>>(factor_strides[0] * factor_shape[0]);
        for (int16_t bb = 0; bb <= max_bb; bb++)
            for (int16_t db = 0; db <= 2; db++)
                for (int16_t bk = 0; bk <= max_bk; bk++)
                    for (int16_t dk = 0; dk <= 2; dk++)
                        for (int16_t fq = 0; fq <= max_bh; fq++)
                            for (int16_t iq = 0; iq <= max_bh; iq++)
                                for (int16_t dq = 0; dq <= max_dh; dq++)
                                    (*factors)[bb * factor_strides[0] +
                                               db * factor_strides[1] +
                                               bk * factor_strides[2] +
                                               dk * factor_strides[3] +
                                               fq * factor_strides[4] +
                                               iq * factor_strides[5] +
                                               dq * factor_strides[6]] =
                                        (FL)SU2Matrix<FL>::cg().wigner_9j(
                                            bk + dk - 1, 1 - (dk & 1), bk, iq,
                                            dq, fq, bb + db - 1, 1 - (db & 1),
                                            bb) *
                                        (FL)sqrtl((bk + 1) * (fq + 1) *
                                                  (bb + db) * (2 - (db & 1))) *
                                        (FL)(1 -
                                             ((((bk + dk - 1) & 1) & (dq & 1))
                                              << 1));
    }
    void fill_csr_matrix(const vector<vector<MKL_INT>> &col_idxs,
                         const vector<vector<FL>> &values,
                         GCSRMatrix<FL> &mat) const {
        const FP sparse_max_nonzero_ratio = 0.25;
        assert(mat.data == nullptr);
        assert(mat.alloc != nullptr);
        size_t nnz = 0;
        for (auto &xv : values)
            nnz += xv.size();
        mat.nnz = (MKL_INT)nnz;
        if ((size_t)mat.nnz != nnz)
            throw runtime_error(
                "NNZ " + Parsing::to_string(nnz) +
                " exceeds MKL_INT. Rebuild with -DUSE_MKL64=ON.");
        if (mat.nnz < mat.size() &&
            mat.nnz <= sparse_max_nonzero_ratio * mat.size()) {
            mat.allocate();
            for (size_t i = 0, k = 0; i < values.size(); i++) {
                mat.rows[i] = (MKL_INT)k;
                memcpy(&mat.data[k], &values[i][0],
                       sizeof(FL) * values[i].size());
                memcpy(&mat.cols[k], &col_idxs[i][0],
                       sizeof(MKL_INT) * col_idxs[i].size());
                k += values[i].size();
            }
            mat.rows[values.size()] = mat.nnz;
        } else {
            mat.nnz = mat.size();
            mat.allocate();
            for (size_t i = 0; i < values.size(); i++)
                for (size_t j = 0; j < values[i].size(); j++)
                    mat.data[col_idxs[i][j] + i * mat.n] = values[i][j];
        }
    }
    void build_operator_matrices(
        const shared_ptr<HDRT<S, ElemOpTypes::SU2>> &hdrt,
        const vector<shared_ptr<vector<FL>>> &ints,
        const vector<vector<SU2Matrix<FL>>> &site_matrices,
        const vector<shared_ptr<CSRSparseMatrix<S, FL>>> &mats) const {
        int ntg = threading->activate_global();
        vector<vector<vector<int>>> jh(ntg, vector<vector<int>>(2));
        vector<vector<vector<int>>> jket(ntg, vector<vector<int>>(2));
        vector<vector<vector<LL>>> ph(ntg, vector<vector<LL>>(2));
        vector<vector<vector<LL>>> pket(ntg, vector<vector<LL>>(2));
        vector<vector<vector<FL>>> hv(ntg, vector<vector<FL>>(2));
        if (mats.size() == 0)
            return;
        assert(ints.size() == mats.size());
        for (int im = 0; im < mats[0]->info->n; im++) {
            S opdq = mats[0]->info->delta_quantum;
            S qbra = mats[0]->info->quanta[im].get_bra(opdq);
            S qket = mats[0]->info->quanta[im].get_ket();
            // SU2 and fermion factor for exchange:
            //   ket x op -> op x ket when is_right
            FL xf = is_right
                        ? (FL)(SU2Matrix<FL>::cg().phase(
                                   opdq.twos(), qket.twos(), qbra.twos()) *
                               (1 - ((opdq.twos() & qket.twos() & 1) << 1)))
                        : (FL)1.0;
            int imb = drt->q_index(qbra), imk = drt->q_index(qket);
            assert(mats[0]->info->n_states_bra[im] == drt->xs[imb].back());
            assert(mats[0]->info->n_states_ket[im] == drt->xs[imk].back());
            vector<vector<vector<
                vector<pair<pair<int16_t, int16_t>, pair<int16_t, FL>>>>>>
                hm(drt->n_sites,
                   vector<vector<vector<
                       pair<pair<int16_t, int16_t>, pair<int16_t, FL>>>>>(4));
            vector<vector<size_t>> max_d(drt->n_sites, vector<size_t>(4, 0));
            vector<int> kjis(drt->n_sites);
            for (int k = drt->n_sites - 1, ji = 0, jj; k >= 0; k--, ji = jj) {
                for (jj = ji; hdrt->qs[jj][0] == k + 1;)
                    jj++;
                kjis[k] = ji;
                for (int dbra = 0; dbra < 4; dbra++) {
                    hm[k][dbra].resize(jj - ji);
                    for (int jk = ji; jk < jj; jk++) {
                        for (int d = 0; d < hdrt->nd; d++)
                            if (hdrt->jds[jk * hdrt->nd + d] != 0)
                                for (size_t md = 0;
                                     md < (int)site_matrices[k][d].data.size();
                                     md++)
                                    if (site_matrices[k][d].indices[md].first ==
                                        dbra)
                                        hm[k][dbra][jk - ji].push_back(
                                            make_pair(
                                                make_pair(
                                                    site_matrices[k][d].dq,
                                                    site_matrices[k][d]
                                                        .indices[md]
                                                        .second),
                                                make_pair(d, site_matrices[k][d]
                                                                 .data[md])));
                        max_d[k][dbra] =
                            max(max_d[k][dbra], hm[k][dbra][jk - ji].size());
                    }
                }
            }
            vector<vector<vector<MKL_INT>>> col_idxs(
                ints.size(), vector<vector<MKL_INT>>(drt->xs[imb].back()));
            vector<vector<vector<FL>>> values(
                ints.size(), vector<vector<FL>>(drt->xs[imb].back()));
#pragma omp parallel for schedule(dynamic) num_threads(ntg)
            for (LL ibra = 0; ibra < drt->xs[imb].back(); ibra++) {
                const int tid = threading->get_thread_id();
                int pi = 0, pj = pi ^ 1, jbra = imb;
                vector<vector<int>> &xjh = jh[tid], &xjk = jket[tid];
                vector<vector<LL>> &xph = ph[tid], &xpk = pket[tid];
                vector<vector<FL>> &xhv = hv[tid];
                xjh[pi].clear(), xph[pi].clear(), xjk[pi].clear();
                xpk[pi].clear(), xhv[pi].clear();
                for (int i = 0; i < hdrt->n_init_qs; i++) {
                    xjh[pi].push_back(i), xjk[pi].push_back(imk);
                    xph[pi].push_back(
                        i != 0
                            ? xph[pi].back() +
                                  hdrt->xs[(i - 1) * (hdrt->nd + 1) + hdrt->nd]
                            : 0);
                    xpk[pi].push_back(0), xhv[pi].push_back(xf);
                }
                LL pbra = ibra;
                for (int k = drt->n_sites - 1; k >= 0; k--, pi ^= 1, pj ^= 1) {
                    const int16_t dbra =
                        (int16_t)(upper_bound(drt->xs[jbra].begin(),
                                              drt->xs[jbra].end(), pbra) -
                                  1 - drt->xs[jbra].begin());
                    pbra -= drt->xs[jbra][dbra];
                    const int jbv = drt->jds[jbra][dbra];
                    const size_t hsz = xhv[pi].size() * max_d[k][dbra];
                    xjh[pj].reserve(hsz), xjh[pj].clear();
                    xph[pj].reserve(hsz), xph[pj].clear();
                    xjk[pj].reserve(hsz), xjk[pj].clear();
                    xpk[pj].reserve(hsz), xpk[pj].clear();
                    xhv[pj].reserve(hsz), xhv[pj].clear();
                    for (size_t j = 0; j < xjh[pi].size(); j++)
                        for (const auto &md :
                             hm[k][dbra][xjh[pi][j] - kjis[k]]) {
                            const int16_t d = md.second.first;
                            const int jhv =
                                hdrt->jds[xjh[pi][j] * hdrt->nd + d];
                            const int16_t dket = md.first.second;
                            const int jkv = drt->jds[xjk[pi][j]][dket];
                            if (jkv == 0)
                                continue;
                            const int16_t bfq = drt->abc[jbra][1];
                            const int16_t kfq = drt->abc[xjk[pi][j]][1];
                            const int16_t biq = drt->abc[jbv][1];
                            const int16_t kiq = drt->abc[jkv][1];
                            const int16_t mdq = md.first.first;
                            const int16_t mfq = hdrt->qs[xjh[pi][j]][2];
                            const int16_t miq = hdrt->qs[jhv][2];
                            const FL f =
                                (*factors)[bfq * factor_strides[0] +
                                           (biq - bfq + 1) * factor_strides[1] +
                                           kfq * factor_strides[2] +
                                           (kiq - kfq + 1) * factor_strides[3] +
                                           mfq * factor_strides[4] +
                                           miq * factor_strides[5] +
                                           mdq * factor_strides[6]];
                            if (abs(f) < (FP)1E-14)
                                continue;
                            xjk[pj].push_back(jkv);
                            xjh[pj].push_back(jhv);
                            xpk[pj].push_back(drt->xs[xjk[pi][j]][dket] +
                                              xpk[pi][j]);
                            xph[pj].push_back(
                                hdrt->xs[xjh[pi][j] * (hdrt->nd + 1) + d] +
                                xph[pi][j]);
                            xhv[pj].push_back(f * xhv[pi][j] *
                                              md.second.second);
                        }
                    jbra = jbv;
                }
                vector<LL> idxs;
                idxs.reserve(xhv[pi].size());
                for (LL i = 0; i < (LL)xhv[pi].size(); i++)
                    idxs.push_back(i);
                sort(idxs.begin(), idxs.end(), [&xpk, pi](LL a, LL b) {
                    return xpk[pi][a] < xpk[pi][b];
                });
                LL xn = idxs.size() > 0;
                for (LL i = 1; i < (LL)idxs.size(); i++)
                    xn += (xpk[pi][idxs[i]] != xpk[pi][idxs[i - 1]]);
                for (size_t it = 0; it < ints.size(); it++) {
                    col_idxs[it][ibra].reserve(xn);
                    values[it][ibra].reserve(xn);
                }
                for (size_t it = 0; it < ints.size(); it++) {
                    for (LL i = 0; i < (LL)idxs.size(); i++)
                        if (i == 0 ||
                            (xpk[pi][idxs[i]] != xpk[pi][idxs[i - 1]] &&
                             abs(values[it][ibra].back()) > cutoff)) {
                            col_idxs[it][ibra].push_back((int)xpk[pi][idxs[i]]);
                            values[it][ibra].push_back(
                                xhv[pi][idxs[i]] *
                                (*ints[it])[xph[pi][idxs[i]]]);
                        } else {
                            col_idxs[it][ibra].back() = (int)xpk[pi][idxs[i]];
                            values[it][ibra].back() +=
                                xhv[pi][idxs[i]] *
                                (*ints[it])[xph[pi][idxs[i]]];
                        }
                    assert(col_idxs[it][ibra].size() <= xn &&
                           values[it][ibra].size() <= xn);
                }
            }
            for (size_t it = 0; it < ints.size(); it++)
                fill_csr_matrix(col_idxs[it], values[it],
                                *mats[it]->csr_data[im]);
        }
        threading->activate_normal();
    }
    void build_complementary_site_ops(
        OpNames op_name, const set<S> &iqs, const vector<uint16_t> &idxs,
        const vector<shared_ptr<CSRSparseMatrix<S, FL>>> &mats) const {
        if (mats.size() == 0)
            return;
        const map<OpNames, vector<int16_t>> op_map =
            map<OpNames, vector<int16_t>>{{OpNames::H, vector<int16_t>{2, 4}},
                                          {OpNames::R, vector<int16_t>{1, 3}},
                                          {OpNames::RD, vector<int16_t>{1, 3}},
                                          {OpNames::P, vector<int16_t>{2}},
                                          {OpNames::PD, vector<int16_t>{2}},
                                          {OpNames::Q, vector<int16_t>{2}}};
        vector<pair<S, pair<int16_t, int16_t>>> iop_qs;
        for (auto &iq : iqs)
            for (int16_t i : op_map.at(op_name))
                for (int16_t j = 1; j <= i; j++)
                    iop_qs.push_back(make_pair(iq, make_pair(j, i)));
        shared_ptr<HDRT<S, ElemOpTypes::SU2>> hdrt =
            make_shared<HDRT<S, ElemOpTypes::SU2>>(n_orbs, iop_qs,
                                                   drt->orb_sym);
        vector<shared_ptr<SpinPermScheme>> schemes;
        vector<shared_ptr<GeneralFCIDUMP<FL>>> gfds;
        vector<string> std_exprs;
        if (this->gfd != nullptr)
            gfds.push_back(this->gfd), std_exprs = this->gfd->exprs;
        if (op_name == OpNames::H && this->gfd == nullptr) {
            shared_ptr<GeneralFCIDUMP<FL>> gfd =
                make_shared<GeneralFCIDUMP<FL>>(ElemOpTypes::SU2);
            gfd->exprs.push_back("((C+(C+D)0)1+D)0");
            gfd->indices.push_back(vector<uint16_t>());
            gfd->data.push_back(vector<FL>());
            auto *idx = &gfd->indices.back();
            auto *dt = &gfd->data.back();
            array<uint16_t, 4> arr;
            for (arr[0] = 0; arr[0] < n_orbs; arr[0]++)
                for (arr[1] = 0; arr[1] < n_orbs; arr[1]++)
                    for (arr[2] = 0; arr[2] < n_orbs; arr[2]++)
                        for (arr[3] = 0; arr[3] < n_orbs; arr[3]++) {
                            const FL v =
                                is_right ? fcidump->v(n_total_orbs - 1 - arr[0],
                                                      n_total_orbs - 1 - arr[3],
                                                      n_total_orbs - 1 - arr[1],
                                                      n_total_orbs - 1 - arr[2])
                                         : fcidump->v(arr[0], arr[3], arr[1],
                                                      arr[2]);
                            if (abs(v) > cutoff) {
                                idx->insert(idx->end(), arr.begin(), arr.end());
                                dt->push_back(v);
                            }
                        }
            gfd->exprs.push_back("(C+D)0");
            gfd->indices.push_back(vector<uint16_t>());
            gfd->data.push_back(vector<FL>());
            idx = &gfd->indices.back(), dt = &gfd->data.back();
            for (arr[0] = 0; arr[0] < n_orbs; arr[0]++)
                for (arr[1] = 0; arr[1] < n_orbs; arr[1]++) {
                    const FL v = is_right
                                     ? fcidump->t(n_total_orbs - 1 - arr[0],
                                                  n_total_orbs - 1 - arr[1])
                                     : fcidump->t(arr[0], arr[1]);
                    if (abs(v) > cutoff) {
                        idx->insert(idx->end(), arr.begin(), arr.begin() + 2);
                        dt->push_back((FL)sqrtl(2) * v);
                    }
                }
            std_exprs = gfd->exprs;
            gfds.push_back(gfd->adjust_order(schemes, true, true));
        } else if (op_name == OpNames::R || op_name == OpNames::RD) {
            for (uint16_t ix : idxs) {
                shared_ptr<GeneralFCIDUMP<FL>> gfd =
                    make_shared<GeneralFCIDUMP<FL>>(ElemOpTypes::SU2);
                gfd->exprs.push_back(op_name == OpNames::R ? "((C+D)0+D)1"
                                                           : "(C+(C+D)0)1");
                gfd->indices.push_back(vector<uint16_t>());
                gfd->data.push_back(vector<FL>());
                auto *idx = &gfd->indices.back();
                auto *dt = &gfd->data.back();
                array<uint16_t, 3> arr;
                for (arr[0] = 0; arr[0] < n_orbs; arr[0]++)
                    for (arr[1] = 0; arr[1] < n_orbs; arr[1]++)
                        for (arr[2] = 0; arr[2] < n_orbs; arr[2]++) {
                            const FL v =
                                op_name == OpNames::R
                                    ? (is_right
                                           ? fcidump->v(
                                                 ix, n_total_orbs - 1 - arr[2],
                                                 n_total_orbs - 1 - arr[0],
                                                 n_total_orbs - 1 - arr[1])
                                           : fcidump->v(ix, arr[2], arr[0],
                                                        arr[1]))
                                    : (is_right
                                           ? fcidump->v(
                                                 ix, n_total_orbs - 1 - arr[0],
                                                 n_total_orbs - 1 - arr[2],
                                                 n_total_orbs - 1 - arr[1])
                                           : fcidump->v(ix, arr[0], arr[2],
                                                        arr[1]));
                            if (abs(v) > cutoff) {
                                idx->insert(idx->end(), arr.begin(), arr.end());
                                dt->push_back(v);
                            }
                        }
                gfd->exprs.push_back(op_name == OpNames::R ? "D" : "C");
                gfd->indices.push_back(vector<uint16_t>());
                gfd->data.push_back(vector<FL>());
                idx = &gfd->indices.back(), dt = &gfd->data.back();
                for (arr[0] = 0; arr[0] < n_orbs; arr[0]++) {
                    const FL v = is_right
                                     ? fcidump->t(ix, n_total_orbs - 1 - arr[0])
                                     : fcidump->t(ix, arr[0]);
                    if (abs(v) > cutoff) {
                        idx->push_back(arr[0]);
                        dt->push_back((FL)(sqrtl(2) / 4.0) * v);
                    }
                }
                std_exprs = gfd->exprs;
                gfds.push_back(gfd->adjust_order(schemes, true, true));
            }
        } else if (op_name == OpNames::P || op_name == OpNames::PD) {
            const int16_t iq = (*iqs.begin()).twos();
            for (int ixx = 0; ixx < (int)idxs.size(); ixx += 2) {
                const uint16_t ix0 = idxs[ixx], ix1 = idxs[ixx + 1];
                shared_ptr<GeneralFCIDUMP<FL>> gfd =
                    make_shared<GeneralFCIDUMP<FL>>(ElemOpTypes::SU2);
                gfd->exprs.push_back(op_name == OpNames::P
                                         ? (iq == 0 ? "(D+D)0" : "(D+D)2")
                                         : (iq == 0 ? "(C+C)0" : "(C+C)2"));
                gfd->indices.push_back(vector<uint16_t>());
                gfd->data.push_back(vector<FL>());
                auto *idx = &gfd->indices.back();
                auto *dt = &gfd->data.back();
                array<uint16_t, 2> arr;
                for (arr[0] = 0; arr[0] < n_orbs; arr[0]++)
                    for (arr[1] = 0; arr[1] < n_orbs; arr[1]++) {
                        const FL v =
                            op_name == OpNames::P
                                ? (is_right
                                       ? (FL)(iq == 0 ? 1.0 : -1.0) *
                                             fcidump->v(
                                                 ix0, n_total_orbs - 1 - arr[0],
                                                 ix1, n_total_orbs - 1 - arr[1])
                                       : fcidump->v(ix0, arr[0], ix1, arr[1]))
                                : (is_right
                                       ? (FL)(iq == 0 ? 1.0 : -1.0) *
                                             fcidump->v(
                                                 ix0, n_total_orbs - 1 - arr[1],
                                                 ix1, n_total_orbs - 1 - arr[0])
                                       : fcidump->v(ix0, arr[1], ix1, arr[0]));
                        if (abs(v) > cutoff) {
                            idx->insert(idx->end(), arr.begin(), arr.end());
                            dt->push_back(v);
                        }
                    }
                std_exprs = gfd->exprs;
                gfds.push_back(gfd->adjust_order(schemes, true, true));
            }
        } else if (op_name == OpNames::Q) {
            const int16_t iq = (*iqs.begin()).twos();
            for (int ixx = 0; ixx < (int)idxs.size(); ixx += 2) {
                const uint16_t ix0 = idxs[ixx], ix1 = idxs[ixx + 1];
                shared_ptr<GeneralFCIDUMP<FL>> gfd =
                    make_shared<GeneralFCIDUMP<FL>>(ElemOpTypes::SU2);
                gfd->exprs.push_back(iq == 0 ? "(C+D)0" : "(C+D)2");
                gfd->indices.push_back(vector<uint16_t>());
                gfd->data.push_back(vector<FL>());
                auto *idx = &gfd->indices.back();
                auto *dt = &gfd->data.back();
                array<uint16_t, 2> arr;
                for (arr[0] = 0; arr[0] < n_orbs; arr[0]++)
                    for (arr[1] = 0; arr[1] < n_orbs; arr[1]++) {
                        const FL v =
                            iq == 0
                                ? (is_right
                                       ? (FL)2.0 * fcidump->v(ix0, ix1,
                                                              n_total_orbs - 1 -
                                                                  arr[0],
                                                              n_total_orbs - 1 -
                                                                  arr[1]) -
                                             fcidump->v(
                                                 ix0, n_total_orbs - 1 - arr[1],
                                                 n_total_orbs - 1 - arr[0], ix1)
                                       : (FL)2.0 * fcidump->v(ix0, ix1, arr[0],
                                                              arr[1]) -
                                             fcidump->v(ix0, arr[1], arr[0],
                                                        ix1))
                                : (is_right
                                       ? (FL)(-1.0) *
                                             fcidump->v(
                                                 ix0, n_total_orbs - 1 - arr[1],
                                                 n_total_orbs - 1 - arr[0], ix1)
                                       : fcidump->v(ix0, arr[1], arr[0], ix1));
                        if (abs(v) > cutoff) {
                            idx->insert(idx->end(), arr.begin(), arr.end());
                            dt->push_back(v);
                        }
                    }
                std_exprs = gfd->exprs;
                gfds.push_back(gfd->adjust_order(schemes, true, true));
            }
        }
        schemes.reserve(std_exprs.size());
        for (size_t ix = 0; ix < std_exprs.size(); ix++)
            schemes.push_back(
                make_shared<SpinPermScheme>(SpinPermScheme::initialize_su2(
                    SpinPermRecoupling::count_cds(std_exprs[ix]), std_exprs[ix],
                    false, true)));
        hdrt->initialize_steps(schemes);
        hdrt->initialize();
        vector<shared_ptr<vector<FL>>> ints =
            fill_integral_data(hdrt, schemes, gfds);
        vector<vector<SU2Matrix<FL>>> site_matrices(drt->n_sites);
        for (int i = 0; i < drt->n_sites; i++) {
            for (int d = 0; d < hdrt->nd; d++)
                site_matrices[i].push_back(
                    SU2Matrix<FL>::build_matrix(hdrt->d_expr[d].first)
                        .expand());
        }
        build_operator_matrices(hdrt, ints, site_matrices, mats);
    }
    void get_site_ops(
        uint16_t m,
        unordered_map<shared_ptr<OpExpr<S>>, shared_ptr<SparseMatrix<S, FL>>>
            &ops) const override {
        shared_ptr<SparseMatrix<S, FL>> zero =
            make_shared<SparseMatrix<S, FL>>(nullptr);
        zero->factor = 0.0;
        set<S> h_qs, r_qs, rd_qs, p0_qs, p1_qs, pd0_qs, pd1_qs, q0_qs, q1_qs;
        vector<uint16_t> h_idxs, r_idxs, rd_idxs, p0_idxs, p1_idxs, pd0_idxs,
            pd1_idxs, q0_idxs, q1_idxs;
        vector<shared_ptr<CSRSparseMatrix<S, FL>>> h_mats, r_mats, rd_mats,
            p0_mats, p1_mats, pd0_mats, pd1_mats, q0_mats, q1_mats;
        for (auto &p : ops) {
            OpElement<S, FL> &op =
                *dynamic_pointer_cast<OpElement<S, FL>>(p.first);
            shared_ptr<VectorAllocator<FP>> d_alloc =
                make_shared<VectorAllocator<FP>>();
            shared_ptr<CSRSparseMatrix<S, FL>> mat =
                make_shared<CSRSparseMatrix<S, FL>>();
            mat->initialize(BigSite<S, FL>::find_site_op_info(op.q_label));
            for (int l = 0; l < mat->info->n; l++)
                mat->csr_data[l]->alloc = d_alloc;
            p.second = mat;
            uint8_t s;
            switch (op.name) {
            case OpNames::P:
                s = op.site_index.ss();
                (s == 0 ? p0_qs : p1_qs).insert(op.q_label);
                (s == 0 ? p0_idxs : p1_idxs).push_back(op.site_index[0]);
                (s == 0 ? p0_idxs : p1_idxs).push_back(op.site_index[1]);
                (s == 0 ? p0_mats : p1_mats).push_back(mat);
                break;
            case OpNames::PD:
                s = op.site_index.ss();
                (s == 0 ? pd0_qs : pd1_qs).insert(op.q_label);
                (s == 0 ? pd0_idxs : pd1_idxs).push_back(op.site_index[0]);
                (s == 0 ? pd0_idxs : pd1_idxs).push_back(op.site_index[1]);
                (s == 0 ? pd0_mats : pd1_mats).push_back(mat);
                break;
            case OpNames::Q:
                s = op.site_index.ss();
                (s == 0 ? q0_qs : q1_qs).insert(op.q_label);
                (s == 0 ? q0_idxs : q1_idxs).push_back(op.site_index[0]);
                (s == 0 ? q0_idxs : q1_idxs).push_back(op.site_index[1]);
                (s == 0 ? q0_mats : q1_mats).push_back(mat);
                break;
            case OpNames::R:
                r_qs.insert(op.q_label);
                r_idxs.push_back(op.site_index[0]);
                r_mats.push_back(mat);
                break;
            case OpNames::RD:
                rd_qs.insert(op.q_label);
                rd_idxs.push_back(op.site_index[0]);
                rd_mats.push_back(mat);
                break;
            case OpNames::H:
                h_qs.insert(op.q_label);
                h_idxs.push_back(0);
                h_mats.push_back(mat);
                break;
            default:
                assert(false);
                break;
            }
        }
        build_complementary_site_ops(OpNames::H, h_qs, h_idxs, h_mats);
        build_complementary_site_ops(OpNames::R, r_qs, r_idxs, r_mats);
        build_complementary_site_ops(OpNames::RD, rd_qs, rd_idxs, rd_mats);
        build_complementary_site_ops(OpNames::P, p0_qs, p0_idxs, p0_mats);
        build_complementary_site_ops(OpNames::P, p1_qs, p1_idxs, p1_mats);
        build_complementary_site_ops(OpNames::PD, pd0_qs, pd0_idxs, pd0_mats);
        build_complementary_site_ops(OpNames::PD, pd1_qs, pd1_idxs, pd1_mats);
        build_complementary_site_ops(OpNames::Q, q0_qs, q0_idxs, q0_mats);
        build_complementary_site_ops(OpNames::Q, q1_qs, q1_idxs, q1_mats);
    }
};

} // namespace block2
